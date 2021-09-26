#pragma once

#include "GameFramework/Actor.h"
#include "FGGridActor.generated.h"

constexpr double Sqrt2 = 1.4142135623730950488016887242097;

struct IVec2
{
	int32 x,y;

	bool IsCardinal() const
	{
		return (x==0 && y!=0) || (y==0 && x!=0);
	}

	bool IsDiagonal() const
	{
		return x != 0 && y != 0;
	}

	bool operator==(IVec2 other) const
	{
		return (other.x == x) && (other.y == y); 
	}

	IVec2 operator-(IVec2 other) const
	{
		return IVec2{x-other.x, y-other.y};
	}
};

enum eDir
{		
	North,
    South,
    West,
    East,
    Northwest,
    Northeast,
    Southwest,
    Southeast,
	Nil,
};

/*
const IVec2 UpLeft =		{-1,1};
const IVec2 UpRight =	{1,1};
const IVec2 DownLeft =	{-1,-1};
const IVec2 DownRight = {1,-1};
const IVec2 Up =		{0,1};
const IVec2 Down =	{0,-1};
const IVec2 Left =	{-1,0};
const IVec2 Right =	{1,0};
*/

USTRUCT(BlueprintType)
struct FFGTileinfo
{
	GENERATED_BODY()
public:
	bool ApproachDirs[4] = { false, false, false, false};

	int32 DirectionValues[8] = {0,0,0,0,0,0,0,0};
	
	UPROPERTY(BlueprintReadWrite, Category = "Tile")	
	bool bBlock = false;
};

class UStaticMeshComponent;
class UStaticMesh;
class UStaticMeshDescription;

UCLASS()
class FGAI_2_API AFGGridActor : public AActor
{
	GENERATED_BODY()
public:
	AFGGridActor();

	virtual void BeginPlay() override;

	/*
	* Called whenever placed in the editor or world, having its transform changed etc.
	* Responsible for eventually calling the infamous ConstructionScript in blueprint.
	*/
	virtual void OnConstruction(const FTransform& Transform) override;
	

	UPROPERTY()
	UStaticMeshComponent* StaticMeshComponent = nullptr;

	UPROPERTY()
	UStaticMeshComponent* BlockStaticMeshComponent = nullptr;

	UPROPERTY()
	UStaticMesh* GridMesh = nullptr;

	UPROPERTY()
	UStaticMesh* BlockMesh = nullptr;

	UPROPERTY()
	UStaticMeshDescription* MeshDescription = nullptr;

	UPROPERTY()
	UStaticMeshDescription* BlockMeshDescription = nullptr;

	UFUNCTION(BlueprintPure, Category = "Grid")
	FVector GetWorldLocationFromXY(int32 TileX, int32 TileY) const;

	UFUNCTION(BlueprintPure, Category = "Grid")
	bool GetXYFromWorldLocation(const FVector& WorldLocation, int32& TileX, int32& TileY) const;

	UFUNCTION(BlueprintPure, Category = "Grid")
	int32 GetTileIndexFromWorldLocation(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Grid")
	bool TransformWorldLocationToTileLocation(const FVector& InWorldLocation, FVector& OutTileWorldLocation) const;

	/*
	* Returns a list of indices correlating to the location of a tile within the TileList
	*/
	void GetOverlappingTiles(const FVector& Origin, const FVector& Extent, TArray<int32>& OutOverlappingTiles) const;

	void DrawBlocks();

	void UpdateBlockingTiles();

	void GenerateGrid();

	bool IsWorldLocationInsideGrid(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Grid")
	bool GetTileIndexFromXY(int32 TileX, int32 TileY, int32& OutTileIndex) const;
	bool GetXYFromTileIndex(int32& OutTileX, int32& OutTileY, const int32& TileIndex) const;
	bool IsTileIndexValid(int32 TileIndex) const;


	struct TileEntry
	{
		int32 GScore = 0;
		int32 FScore = 0;
		int32 Parent = -1;
	};

	IVec2 Directions[9] = {
		{0,-1}, //North
		{0,1},  //South
		{-1,0}, //West
		{1,0},  //East
		{-1,-1},//NorthWest
		{1,-1}, //NorthEast
		{-1,1}, //SouthWest
		{1,1},  //SouthEast
		{0,0}
	};

	UFUNCTION(BlueprintCallable)
	TArray<int32> FindPath(const int32& start, const int32& goal);
	bool IsObstacle(int32 X, int32 Y);
	void JPSPreProcess();

	void VisualizePath(const TArray<int32>& path, const TArray<int32>& GScores);
	TArray<int32> ConstructPath(const TArray<int32>& Parent, const int32& Goal);
	UFUNCTION(BlueprintCallable)
	TArray<int32> JPSRuntime(int32 Start, int32 Goal);
	
	
#if WITH_EDITOR
	/*
	* This is called whenever a property, on this Actor, is edited in the editor.
	* Only available in the editor. If you forget WITH_EDITOR you will get a compile error when compiling the non-editor build
	*/
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR

	UFUNCTION(BlueprintPure, Category = "Grid")
	float GetTileSizeHalf() const { return TileSize * 0.5f; }
	UFUNCTION(BlueprintPure, Category = "Grid")
	int32 GetNumTiles() const { return Width * Height; }
	UFUNCTION(BlueprintPure, Category = "Grid")
	float GetGridSize() const { return GetNumTiles() * TileSize; }

	UFUNCTION(BlueprintPure, Category = "Grid")
	float GetHalfWidth() const { return static_cast<float>(Width) * 0.5f; }
	UFUNCTION(BlueprintPure, Category = "Grid")
	float GetHalfHeight() const { return static_cast<float>(Height) * 0.5f; }

	UFUNCTION(BlueprintPure, Category = "Grid")
	float GetWidthSize() const { return (static_cast<float>(Width) * GetTileSizeHalf()) + BorderSize; }
	UFUNCTION(BlueprintPure, Category = "Grid")
	float GetHeightSize() const { return (static_cast<float>(Height) * GetTileSizeHalf()) + BorderSize; }

	UFUNCTION(BlueprintPure, Category = "Grid")
	FVector GetWidthExtends() const { return FVector(BorderSize, GetHeightSize(), BorderSize); }
	UFUNCTION(BlueprintPure, Category = "Grid")
	FVector GetHeightExtends() const { return FVector(GetWidthSize(), BorderSize, BorderSize); }

	/*
	* Initializes to the size of the number of tiles in the grid. 
	*/
	UPROPERTY(BlueprintReadOnly, Category = "Grid")
	TArray<FFGTileinfo> TileList;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Grid, meta = (ClampMin = 1))
	int Width = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Grid, meta = (ClampMin = 1))
	int Height = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Grid, meta = (ClampMin = 0.1))
	float BorderSize = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Grid)
	float TileSize = 500.0f;
};
