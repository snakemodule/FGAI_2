#include "FGGridActor.h"

#include "DrawDebugHelpers.h"
#include "FGGridBlockComponent.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshDescription.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "FGAI_2/AStar/PriorityQueue.h"
#include "Kismet/GameplayStatics.h"

AFGGridActor::AFGGridActor()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	StaticMeshComponent->SetupAttachment(RootComponent);
	StaticMeshComponent->SetCastShadow(false);

	BlockStaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockStaticMeshComponent"));
	BlockStaticMeshComponent->SetupAttachment(RootComponent);
	BlockStaticMeshComponent->SetCastShadow(false);
}

void AFGGridActor::BeginPlay()
{
	Super::BeginPlay();

	JPSPreProcess();

	//TArray<int32> path = JPSRuntime(36, 7);
	//TArray<int32> path = FindPath(36, 7);
}

void AFGGridActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (TileList.Num() == 0)
	{
		/*
		* If TileList is empty it probably means we just placed one in the level, so let's initialize it.
		*/

		TileList.SetNum(GetNumTiles());
	}

	GenerateGrid();

	DrawBlocks();
}

FVector AFGGridActor::GetWorldLocationFromXY(int32 TileX, int32 TileY) const
{
	const float X = ((static_cast<float>(TileX) - GetHalfWidth()) * TileSize) + GetTileSizeHalf();
	const float Y = ((static_cast<float>(TileY) - GetHalfHeight()) * TileSize) + GetTileSizeHalf();

	return GetActorTransform().TransformPosition(FVector(X, Y, 0));
}

bool AFGGridActor::GetXYFromWorldLocation(const FVector& WorldLocation, int32& TileX, int32& TileY) const
{
	if (!IsWorldLocationInsideGrid(WorldLocation))
		return false;

	const FVector RelativeGridLocation = GetActorTransform().InverseTransformPositionNoScale(WorldLocation);

	const float HeightOffset = (Height % 2) == 1 ? 0.5f : 0.0f;
	const float WidthOffset = (Width % 2) == 1 ? 0.5f : 0.0f;

	const float X = FMath::FloorToInt(WidthOffset + (RelativeGridLocation.X / TileSize)) + GetHalfWidth() - WidthOffset;
	const float Y = FMath::FloorToInt(HeightOffset + (RelativeGridLocation.Y / TileSize)) + GetHalfHeight() -
		HeightOffset;

	TileX = FMath::Clamp(static_cast<int32>(X), 0, Width - 1);
	TileY = FMath::Clamp(static_cast<int32>(Y), 0, Height - 1);

	return true;
}

int32 AFGGridActor::GetTileIndexFromWorldLocation(const FVector& WorldLocation) const
{
	int32 X = 0, Y = 0;
	int32 ResultTileIndex;
	if (GetXYFromWorldLocation(WorldLocation, X, Y) && GetTileIndexFromXY(X, Y, ResultTileIndex))
	{
		return ResultTileIndex;
	}

	return 0;
}

bool AFGGridActor::TransformWorldLocationToTileLocation(const FVector& InWorldLocation,
                                                        FVector& OutTileWorldLocation) const
{
	if (!IsWorldLocationInsideGrid(InWorldLocation))
		return false;

	int32 X = 0, Y = 0;
	if (GetXYFromWorldLocation(InWorldLocation, X, Y))
	{
		OutTileWorldLocation = GetWorldLocationFromXY(X, Y);
		return true;
	}

	return false;
}



void AFGGridActor::GetOverlappingTiles(const FVector& Origin, const FVector& Extent,
                                       TArray<int32>& OutOverlappingTiles) const
{
	const FBox BlockBox = FBox::BuildAABB(Origin, Extent);

	const FVector TileExtent(GetTileSizeHalf(), GetTileSizeHalf(), GetTileSizeHalf());

	FBox TileBox;

	for (int32 Y = Height - 1; Y >= 0; --Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const FVector TileWorldLocation = GetWorldLocationFromXY(X, Y);

			TileBox = FBox::BuildAABB(TileWorldLocation, TileExtent);
			if (TileBox.IntersectXY(BlockBox))
			{
				int32 ArrayIndex;
				GetTileIndexFromXY(X, Y, ArrayIndex);
				OutOverlappingTiles.Add(ArrayIndex);
			}
		}
	}
}

void AFGGridActor::DrawBlocks()
{
	const int32 NumBlocks = TileList.Num();

	if (NumBlocks == 0)
		return;

	if (BlockMeshDescription == nullptr)
		BlockMeshDescription = UStaticMesh::CreateStaticMeshDescription(this);

	if (BlockMesh == nullptr)
		BlockMesh = NewObject<UStaticMesh>(this, UStaticMesh::StaticClass());

	BlockMeshDescription->Empty();

	BlockStaticMeshComponent->SetStaticMesh(nullptr);

	FPolygonGroupID BlockPGID = BlockMeshDescription->CreatePolygonGroup();
	FPolygonID PID;

	const float BlockSize = TileSize * 0.25f;
	const FVector BlockExtent = FVector(BlockSize, BlockSize, BlockSize * 0.25f);

	for (int32 Y = Height - 1; Y >= 0; --Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const FVector TileRelativeLocation = GetActorTransform().InverseTransformPositionNoScale(
				GetWorldLocationFromXY(X, Y));
			int32 ArrayIndex;
			GetTileIndexFromXY(X, Y, ArrayIndex);
			const bool bIsBlocked = TileList[ArrayIndex].bBlock;

			if (bIsBlocked)
			{
				BlockMeshDescription->CreateCube(TileRelativeLocation, BlockExtent, BlockPGID, PID, PID, PID, PID, PID,
				                                 PID);
			}
		}
	}

	if (!BlockMeshDescription->IsEmpty())
	{
		TArray<UStaticMeshDescription*> BlockMeshDescriptionList;
		BlockMeshDescriptionList.Add(BlockMeshDescription);
		BlockMesh->BuildFromStaticMeshDescriptions(BlockMeshDescriptionList);
		BlockStaticMeshComponent->SetStaticMesh(BlockMesh);
	}
}

void AFGGridActor::UpdateBlockingTiles()
{
	TArray<UFGGridBlockComponent*> AllBlocks;
	GetComponents(AllBlocks);

	TileList.Empty();
	TileList.SetNum(GetNumTiles());

	TArray<int32> BlockIndices;

	for (const auto Block : AllBlocks)
	{
		const FVector Origin = Block->GetComponentLocation();
		const FVector Extents = Block->Extents * 0.5f;

		BlockIndices.Reset();
		GetOverlappingTiles(Origin, Extents, BlockIndices);

		for (int32 Index = 0, Num = BlockIndices.Num(); Index < Num; ++Index)
		{
			TileList[BlockIndices[Index]].bBlock = true;
		}
	}

	DrawBlocks();
}

void AFGGridActor::GenerateGrid()
{
	if (Width < 1 || Height < 1)
		return;

	if (MeshDescription == nullptr)
		MeshDescription = UStaticMesh::CreateStaticMeshDescription(this);

	if (GridMesh == nullptr)
		GridMesh = NewObject<UStaticMesh>(this, UStaticMesh::StaticClass());

	MeshDescription->Empty();

	FPolygonGroupID PGID = MeshDescription->CreatePolygonGroup();
	FPolygonID PID;

	float Location_X = -((Width * TileSize) * 0.5f);
	float Location_Y = -((Height * TileSize) * 0.5f);

	for (int X = 0; X < Width + 1; ++X)
	{
		float LocationOffset = X * TileSize;
		FVector Center = FVector(Location_X + LocationOffset, 0.0f, 0.0f);
		FVector Test1 = FVector(BorderSize, GetHeightSize(), BorderSize);
		MeshDescription->CreateCube(Center, GetWidthExtends(), PGID, PID, PID, PID, PID, PID, PID);
	}

	for (int Y = 0; Y < Height + 1; ++Y)
	{
		float LocationOffset = Y * TileSize;
		FVector Center = FVector(0.0f, Location_Y + LocationOffset, BorderSize);
		FVector Test = FVector(GetWidthSize(), BorderSize, BorderSize);
		MeshDescription->CreateCube(Center, GetHeightExtends(), PGID, PID, PID, PID, PID, PID, PID);
	}

	TArray<UStaticMeshDescription*> MeshDescriptionList;
	MeshDescriptionList.Add(MeshDescription);
	GridMesh->BuildFromStaticMeshDescriptions(MeshDescriptionList);
	StaticMeshComponent->SetStaticMesh(GridMesh);
}

bool AFGGridActor::IsWorldLocationInsideGrid(const FVector& WorldLocation) const
{
	const FVector RelativeGridLocation = GetActorTransform().InverseTransformPositionNoScale(WorldLocation);

	if (RelativeGridLocation.X < -GetWidthSize())
		return false;
	else if (RelativeGridLocation.X > GetWidthSize())
		return false;
	else if (RelativeGridLocation.Y < -GetHeightSize())
		return false;
	else if (RelativeGridLocation.Y > GetHeightSize())
		return false;

	return true;
}

bool AFGGridActor::GetTileIndexFromXY(int32 TileX, int32 TileY, int32& OutTileIndex) const
{
	if (TileX < 0 || TileX >= Width)
		return false;


	if (TileY < 0 || TileY >= Height)
		return false;

	const int32 TileIndex = (TileY * Width) + TileX;


	if (!IsTileIndexValid(TileIndex))
	{
		OutTileIndex = -1;
		return false;
	}

	OutTileIndex = TileIndex;

	return true;
}

bool AFGGridActor::GetXYFromTileIndex(int32& OutTileX, int32& OutTileY, const int32& TileIndex) const
{
	if (!IsTileIndexValid(TileIndex))
		return false;
	OutTileX = TileIndex % Width;
	OutTileY = (TileIndex - OutTileX) / Width;
	return true;
}

bool AFGGridActor::IsTileIndexValid(int32 TileIndex) const
{
	const int32 NumTiles = TileList.Num();

	if (TileIndex < 0 || TileIndex >= NumTiles)
	{
		return false;
	}

	return true;
}

#if WITH_EDITOR
void AFGGridActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateBlockingTiles();
}
#endif // WITH_EDITOR

//struct TileEntry
//{
//	Tile tile;
//	TileEntry* parent = nullptr;
//	int32 gScore = 0;
//	int32 hScore = 0;
//	int32 fScore = 0;
//};

TArray<int32> AFGGridActor::FindPath(const int32& start, const int32& goal)
{
	int32 GoalX, GoalY;
	GetXYFromTileIndex(GoalX, GoalY, goal);
	auto Heuristic = [GoalX, GoalY, this](int32 X, int32 Y)-> int32
	{
		const int32 XDiff = FMath::Abs(X - GoalX);
		const int32 YDiff = FMath::Abs(Y - GoalY);
		return XDiff + YDiff;
	};

	PriorityQueue<int32> OpenQueue;
	int32 StartX, StartY;
	GetXYFromTileIndex(StartX, StartY, start);
	OpenQueue.PrioritisedAdd(start, Heuristic(StartX, StartY));

	struct FTileData
	{
		TArray<int32> GScores = TArray<int32>();
		TArray<int32> FScores = TArray<int32>();
		TArray<int32> Parent = TArray<int32>();
	} TileData;

	const int32 NumTiles = GetNumTiles();
	TileData.GScores.Init(0, NumTiles);
	TileData.FScores.Init(0, NumTiles);
	TileData.Parent.Init(-1, NumTiles);

	while (OpenQueue.List.Num() > 0)
	{
		int32 CurrentTileIdx = OpenQueue.PopFirst();

		if (CurrentTileIdx == goal)
		{			
			auto path = ConstructPath(TileData.Parent, CurrentTileIdx);
			VisualizePath(path, TileData.GScores);			
			return path;
		}

		int32 TileX, TileY;
		GetXYFromTileIndex(TileX, TileY, CurrentTileIdx);

		int32 NeighborsXOffset[4] = {0, 0, -1, 1};
		int32 NeighborsYOffset[4] = {-1, 1, 0, 0};
		for (int i = 0; i < 4; ++i) //loop over neighbors
		{
			const int32 NeighborX = TileX + NeighborsXOffset[i];
			const int32 NeighborY = TileY + NeighborsYOffset[i];

			int32 NeighborIdx;
			if (!GetTileIndexFromXY(NeighborX, NeighborY, NeighborIdx)
				|| TileList[NeighborIdx].bBlock
				|| NeighborIdx	== start)
				continue; //impassable tile

			const int32 NewGScore = TileData.GScores[CurrentTileIdx] + 1;
			if (NewGScore < TileData.GScores[NeighborIdx] || TileData.GScores[NeighborIdx] == 0)
			{
				TileData.Parent[NeighborIdx] = CurrentTileIdx;
				TileData.GScores[NeighborIdx] = NewGScore;
				TileData.FScores[NeighborIdx] = NewGScore + Heuristic(NeighborX, NeighborY);

				if (OpenQueue.Contains(NeighborIdx))
					OpenQueue.UpdatePriority(NeighborIdx, TileData.FScores[NeighborIdx]);
				else
					OpenQueue.PrioritisedAdd(NeighborIdx, TileData.FScores[NeighborIdx]);
			}
		}
	}
	return TArray<int32>();
}



bool AFGGridActor::IsObstacle(int32 X, int32 Y)
{
	int32 Idx;
	if (!GetTileIndexFromXY(X, Y, Idx) || TileList[Idx].bBlock)
	{
		return true; 
	}
	return false;
}

void AFGGridActor::JPSPreProcess()
{
	const auto NumTiles = GetNumTiles();
	
#pragma region primary_jump_points
	for (int TileIndex = 0; TileIndex < NumTiles; ++TileIndex)
	{		
		struct BlockCase
		{
			IVec2 Offset;
			struct
			{
				IVec2 N;
				eDir Dir;
			} FNCase1;
			struct
			{
				IVec2 N;
				eDir Dir;
			} FNCase2;
		};

		BlockCase Cases[4] = {
			{Directions[Northwest],{Directions[North], East},{Directions[West],South}},
			{Directions[Northeast],{Directions[North], West},{Directions[East], South}},
			{Directions[Southwest],{Directions[West], North},{Directions[South], East}},
			{Directions[Southeast],{Directions[East], North},{Directions[South], West}}
		};

		int32 CX, CY;
		GetXYFromTileIndex(CX,CY,TileIndex);

		for (int j = 0; j < 4; ++j)
		{
			int32 Idx;
			bool got = GetTileIndexFromXY(CX+Cases[j].Offset.x, CY+Cases[j].Offset.y, Idx);
			if (!got
				|| !TileList[Idx].bBlock)
				continue;

			int32 FNIdx;
			if (GetTileIndexFromXY(CX+Cases[j].FNCase1.N.x, CY+Cases[j].FNCase1.N.y, FNIdx) && !TileList[FNIdx].bBlock)
			{
				const IVec2 ApproachDir = Directions[Cases[j].FNCase1.Dir];
				if (!IsObstacle(CX-ApproachDir.x, CY-ApproachDir.y)) 
					TileList[TileIndex].ApproachDirs[Cases[j].FNCase1.Dir] = true; 					
			}
			if (GetTileIndexFromXY(CX+Cases[j].FNCase2.N.x, CY+Cases[j].FNCase2.N.y, FNIdx) && !TileList[FNIdx].bBlock)
			{
				const IVec2 ApproachDir = Directions[Cases[j].FNCase2.Dir];
				if (!IsObstacle(CX-ApproachDir.x, CY-ApproachDir.y))
					TileList[TileIndex].ApproachDirs[Cases[j].FNCase2.Dir] = true; 
			}
		}		
	}
#pragma endregion

#pragma region CardinalSweeps

	auto CardinalSweep = [this](const int32 X,const int32 Y,
                        eDir Dir,  int32& Distance, bool& jumpPointLastSeen)
	{
		int32 Idx;
		GetTileIndexFromXY(X, Y, Idx);
		
		if(TileList[Idx].bBlock)
		{				
			Distance = -1;
			jumpPointLastSeen = false;
			TileList[Idx].DirectionValues[Dir] = 0;
			return;
		}
			
		Distance = Distance + 1;

		if (jumpPointLastSeen)
			TileList[Idx].DirectionValues[Dir] = Distance;
		else
			TileList[Idx].DirectionValues[Dir] = -Distance;
			
		if (TileList[Idx].ApproachDirs[Dir]) //this is a jump point for westward direction
		{
			Distance = 0;
			jumpPointLastSeen = true;
		}	
	};
	
	//SweepRight_WestwardValues
	for (int Y = 0; Y < Height; ++Y)
	{
		int32 Distance = -1;
		bool bJumpPointLastSeen = false;
		for (int X = 0; X < Width; ++X)
			CardinalSweep(X, Y, eDir::West, Distance, bJumpPointLastSeen);
	}
	
	//SweepLeft_EastwardValues	
	for (int Y = 0; Y < Height; ++Y)
	{
		int32 Distance = -1;
		bool bJumpPointLastSeen = false;
		for (int X = Width - 1; X >= 0; --X)		
			CardinalSweep(X, Y, eDir::East, Distance, bJumpPointLastSeen);
	}
	
	//SweepDown_NorthwardValues
	for (int X = 0; X < Width; ++X)
	{
		int32 Distance = -1;
		bool bJumpPointLastSeen = false;
		for (int Y = 0; Y < Height; ++Y)
			CardinalSweep(X, Y, eDir::North, Distance, bJumpPointLastSeen);
	}
	
	//SweepUp_SouthwardValues
	for (int X = 0; X < Width; ++X)
	{
		int32 Distance = -1;
		bool bJumpPointLastSeen = false;
		for (int Y = Height - 1; Y >= 0; --Y)
			CardinalSweep(X, Y, eDir::South, Distance, bJumpPointLastSeen);
	}	
#pragma endregion

#pragma region Diagonals
	auto DiagonalSweep = [this](const int32 X, const int32 Y,
                    eDir Vertical, eDir Horizontal, eDir Diagonal, FFGTileinfo& CurrentTileInfo)
	{
		const IVec2 DiagonalOffset = Directions[Diagonal];
		
		int32 PrevIdx;				
		GetTileIndexFromXY(X+DiagonalOffset.x, Y+DiagonalOffset.y, PrevIdx);		
			
		if (IsObstacle(X, Y+DiagonalOffset.y)
            || IsObstacle(X+DiagonalOffset.x,Y) || IsObstacle(X+DiagonalOffset.x,Y+DiagonalOffset.y))
		{
			CurrentTileInfo.DirectionValues[Diagonal] = 0;
		}
		else if (!IsObstacle(X,Y+DiagonalOffset.y) && !IsObstacle(X+DiagonalOffset.x, Y)
            && (TileList[PrevIdx].DirectionValues[Vertical] > 0
                ||  TileList[PrevIdx].DirectionValues[Horizontal] > 0))
		{
			CurrentTileInfo.DirectionValues[Diagonal] = 1;
		}
		else
		{
			int JumpDistance = TileList[PrevIdx].DirectionValues[Diagonal];
			if (JumpDistance > 0)
			{
				CurrentTileInfo.DirectionValues[Diagonal] = 1 + JumpDistance;
			}
			else
			{
				CurrentTileInfo.DirectionValues[Diagonal] = -1 + JumpDistance;
			}
		}
	};
	
	
	for (int Y = Height - 1; Y >= 0; --Y)
	{
		for (int X = 0; X < Width; ++X)
		{
			int32 Idx;			
			GetTileIndexFromXY(X, Y, Idx);
			if(!IsObstacle(X,Y))
			{				
				DiagonalSweep(X, Y, eDir::South, eDir::West, eDir::Southwest, TileList[Idx]);
				DiagonalSweep(X, Y, eDir::South, eDir::East, eDir::Southeast, TileList[Idx]);
			}
		}
	}
	
	for (int Y = 0; Y < Height; ++Y)
	{
		for (int X = 0; X < Width; ++X)
		{
			int32 Idx;			
			GetTileIndexFromXY(X, Y, Idx);
			if(!IsObstacle(X,Y))
			{
				DiagonalSweep(X, Y, eDir::North, eDir::West, eDir::Northwest, TileList[Idx]);
				DiagonalSweep(X, Y, eDir::North, eDir::East, eDir::Northeast, TileList[Idx]);
			}
		}
	}		
#pragma endregion

}

void AFGGridActor::VisualizePath(const TArray<int32>& path, const TArray<int32>& GScores)
{
	for (int i = 0; i < GScores.Num(); ++i)
	{
		int32 X, Y;
		if (GScores[i] == 0)
		{
			continue;
		}
		GetXYFromTileIndex(X, Y, i);
		DrawDebugBox(GetWorld(), GetWorldLocationFromXY(X, Y), FVector(50.f), FColor::Red, false, 5, 0, 10);
	}
			
	for (int i = 1; i < path.Num(); ++i)
	{
		int32 X0, Y0, X1, Y1;
		GetXYFromTileIndex(X1, Y1, path[i]);
		GetXYFromTileIndex(X0, Y0, path[i - 1]);
		DrawDebugLine(GetWorld(), GetWorldLocationFromXY(X0, Y0),
                      GetWorldLocationFromXY(X1, Y1), FColor::Green, false, 5, 0, 50);
	}

	for (int i = 0; i < path.Num(); ++i)
	{
		int32 X0, Y0;
		
		GetXYFromTileIndex(X0, Y0, path[i]);
		DrawDebugSphere(GetWorld(), GetWorldLocationFromXY(X0, Y0), 100, 10, FColor::Blue, false, 5, 0, 5);
	}
}

TArray<int32> AFGGridActor::ConstructPath(const TArray<int32>& Parent, const int32& Goal)
{
	TArray<int32> path;

	int32 CurrentIdx = Goal;
	while (CurrentIdx != -1)
	{
		path.Add(CurrentIdx);
		CurrentIdx = Parent[CurrentIdx];
	}
	return path;
}

TArray<int32> AFGGridActor::JPSRuntime(int32 Start, int32 Goal)
{	
	struct SearchDirs
	{
		eDir TravelDir;
		TArray<eDir> ValidDirs;
	};
	SearchDirs ValidDirLookup[9] = {
		{eDir::North, {eDir::East, eDir::Northeast, eDir::North, eDir::Northwest, eDir::West}},
		{eDir::South, {eDir::West, eDir::Southwest, eDir::South, eDir::Southeast, eDir::East}},
		{eDir::West,  {eDir::North, eDir::Northwest, eDir::West, eDir::Southwest, eDir::South}},
		{eDir::East,  {eDir::South, eDir::Southeast, eDir::East, eDir::Northeast, eDir::North}},
	    {eDir::Northwest, {eDir::North, eDir::Northwest, eDir::West}},
		{eDir::Northeast, {eDir::East, eDir::Northeast, eDir::North}},
		{eDir::Southwest, {eDir::West, eDir::Southwest, eDir::South }},
		{eDir::Southeast, {eDir::South, eDir::Southeast, eDir::East }},
		{eDir::Nil, {eDir::North,eDir::South,eDir::West,eDir::East,eDir::Northwest,eDir::Northeast,eDir::Southwest,eDir::Southeast}}
	};

	auto ManhattanDist = [](IVec2 a, IVec2 b)-> int32
	{		
		const int32 XDiff = FMath::Abs(a.x - b.x);
		const int32 YDiff = FMath::Abs(a.y - b.y);
		return XDiff + YDiff;
	};

	int32 GoalX,GoalY;
	GetXYFromTileIndex(GoalX,GoalY, Goal);
	IVec2 GoalXY = {GoalX, GoalY};
	
	int32 StartX, StartY;
	GetXYFromTileIndex(StartX, StartY, Start);
	IVec2 StartXY = {StartX, StartY};
	
	PriorityQueue<int32> OpenQueue;
	OpenQueue.PrioritisedAdd(Start, ManhattanDist(StartXY,GoalXY));

	struct FTileData
	{
		TArray<int32> GScores = TArray<int32>();
		TArray<int32> FScores = TArray<int32>();
		TArray<int32> Parent = TArray<int32>();
	} TileData;

	const int32 NumTiles = GetNumTiles();
	TileData.GScores.Init(0, NumTiles);
	TileData.FScores.Init(0, NumTiles);
	TileData.Parent.Init(-1, NumTiles);
	
	while (OpenQueue.List.Num() > 0)
	{
		int32 CurrentNode = OpenQueue.PopFirst();
		int32 ParentNode = TileData.Parent[CurrentNode];

		int32 CurrentX, CurrentY;
		GetXYFromTileIndex(CurrentX, CurrentY, CurrentNode);

		//DrawDebugBox(GetWorld(), GetWorldLocationFromXY(CurrentX, CurrentY), FVector(50.f), FColor::Red, true, 1, 0, 10);

		
		if (CurrentNode == Goal)
		{			
			auto path = ConstructPath(TileData.Parent, CurrentNode);
			VisualizePath(path, TileData.GScores);
			return path;
		}
			

		//Get Travel direction from parent
		IVec2 TravelDirection;		
		if (TileData.Parent[CurrentNode] == -1)
		{
			TravelDirection = {0,0};
		}
		else
		{			
			int32 ParentX, ParentY;
			GetXYFromTileIndex(ParentX, ParentY, TileData.Parent[CurrentNode]);
			
			const int32 DX = FMath::Clamp(CurrentX-ParentX,-1,1);
			const int32 DY = FMath::Clamp(CurrentY-ParentY,-1,1);
			TravelDirection = {DX, DY};			
		}

		//get directions to check from travel direction
		TArray<eDir> ValidDirections;
		for (int i = 0; i < 9; ++i)
		{
			if (Directions[ValidDirLookup[i].TravelDir] == TravelDirection)
			{
				ValidDirections = ValidDirLookup[i].ValidDirs;
				break;
			}		
		}
		

		for (auto ValidDirection : ValidDirections)
		{
			int32 newSuccessor = -1;
			float givenCost = 0;
			
			IVec2 DirectionVector = Directions[ValidDirection];
			
			

			
			IVec2 CurrentXY = {CurrentX, CurrentY};
			IVec2 GoalDiff = GoalXY - CurrentXY;
		
			bool GoalInExactDirection = false;
			bool GoalInGeneralDirection = false;
			IVec2 GoalDir = {FMath::Clamp(GoalDiff.x, -1, 1),
                       FMath::Clamp(GoalDiff.y, -1, 1)};
			if (GoalDiff.x == 0 || GoalDiff.y == 0 || FMath::Abs(GoalDiff.x) == FMath::Abs(GoalDiff.y)) // cardinal or diagonal
			{				
				if (GoalDir == DirectionVector)
				{
					GoalInExactDirection = true;
					GoalInGeneralDirection = true;
				}
			}
			else 
			{
				if (GoalDir == DirectionVector)
					GoalInGeneralDirection = true;
			}
						
			const int32 DistanceToGoal = ManhattanDist(CurrentXY, GoalXY);
			const int32 DistanceOfThisDirection = FMath::Abs(TileList[CurrentNode].DirectionValues[ValidDirection]);
			if(DirectionVector.IsCardinal() && GoalInExactDirection
				&& DistanceToGoal <= DistanceOfThisDirection)
			{
				newSuccessor = Goal;
				givenCost = DistanceToGoal + TileData.GScores[CurrentNode];
			}
			else  /*&& goal is in general direction see text about middle conditional*/
			{
				const int32 RowDiffToGoal = FMath::Abs(CurrentX-GoalX);
				const int32 ColDiffToGoal = FMath::Abs(CurrentY-GoalY);
				const bool GoalCloserThanJumpDistance =  RowDiffToGoal <= DistanceOfThisDirection
												  || ColDiffToGoal <= DistanceOfThisDirection; 
				if (DirectionVector.IsDiagonal() && GoalInGeneralDirection && GoalCloserThanJumpDistance)
				{
					int minDiff = FMath::Min(RowDiffToGoal,  ColDiffToGoal);
					
					GetTileIndexFromXY(CurrentX+DirectionVector.x*minDiff, CurrentY+DirectionVector.y*minDiff, newSuccessor);
					givenCost = TileData.GScores[CurrentNode] + Sqrt2*minDiff; 
				}
				else if(TileList[CurrentNode].DirectionValues[ValidDirection] > 0) //there is a jump point in this direction
				{
					GetTileIndexFromXY(CurrentX+DirectionVector.x*DistanceOfThisDirection,
									   CurrentY+DirectionVector.y*DistanceOfThisDirection, newSuccessor);
					givenCost = TileData.GScores[CurrentNode];
					if (DirectionVector.IsDiagonal())
						givenCost += Sqrt2*DistanceOfThisDirection;
					else
						givenCost += DistanceOfThisDirection;
				}
			}

			if (newSuccessor != -1)
			{
				if (givenCost < TileData.GScores[newSuccessor] || TileData.GScores[newSuccessor] == 0)
				{
					TileData.Parent[newSuccessor] = CurrentNode;
					TileData.GScores[newSuccessor] = givenCost;
					TileData.FScores[newSuccessor] = givenCost + DistanceToGoal;
					if (OpenQueue.Contains(newSuccessor))
						OpenQueue.UpdatePriority(newSuccessor, TileData.FScores[newSuccessor]);
					else
						OpenQueue.PrioritisedAdd(newSuccessor, TileData.FScores[newSuccessor]);
				}
			}
		}		
	}
	return TArray<int32>();
};


