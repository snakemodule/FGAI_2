#pragma once

#include "Core/Public/Containers/List.h"

template <typename T = int32>
class PriorityQueue
{
public:
	struct ValuePriority
	{
		int32 prio;
		T value;
	};

	TDoubleLinkedList<ValuePriority> List;
	TBitArray<FDefaultBitArrayAllocator> Set;
	
	PriorityQueue()
	{
	}

	void PrioritisedAdd(const T& Value, const int32& Prio)
	{
		ValuePriority ToInsert = ValuePriority{Prio, Value};

		auto CurrentNode = List.GetHead();
		while (CurrentNode != nullptr)
		{
			if (Prio <= CurrentNode->GetValue().prio)
			{
				List.InsertNode(ToInsert, CurrentNode);				
				goto end;
			}
			CurrentNode = CurrentNode->GetNextNode();
		}
		List.AddTail(ToInsert);
end:
		Set.PadToNum(Value+1, false);
		Set[Value] = true;
	};

	//searches for value, deletes and re adds with new prio if needed.
	void UpdatePriority(const T& Value, const int32& Prio)
	{
		auto CurrentNode = List.GetHead();

		while (CurrentNode != nullptr)
		{
			if (Value == CurrentNode->GetValue().value)
			{
				if (Prio != CurrentNode->GetValue().prio)
				{
					List.RemoveNode(CurrentNode);
					PrioritisedAdd(Value, Prio);					
				}
                return;
			}			
			CurrentNode = CurrentNode->GetNextNode();
		}
	}
	
	T PopFirst()
	{
		T Result = List.GetHead()->GetValue().value;
		Set[Result] = false;
		List.RemoveNode(List.GetHead());
		return Result;
	};

	bool Contains(const T& Value)
	{
		return Set.IsValidIndex(Value) && Set[Value] == true;
	};
};
