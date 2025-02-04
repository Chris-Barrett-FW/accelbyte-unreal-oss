// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.
#pragma once

#include "AsyncTasks/OnlineAsyncTaskAccelByte.h"
#include "OnlineSubsystemAccelByteTypes.h"
#include "OnlineFriendsInterfaceAccelByte.h"
#include "Models/AccelByteLobbyModels.h"
#include "Models/AccelByteUserModels.h"
#include "OnlineUserCacheAccelByte.h"

/**
 * Async task to try and read the user's friends list from the backend through the Lobby websocket.
 */
class FOnlineAsyncTaskAccelByteReadFriendsList : public FOnlineAsyncTaskAccelByte
{
public:

	FOnlineAsyncTaskAccelByteReadFriendsList(FOnlineSubsystemAccelByte* const InABInterface, int32 InLocalUserNum, const FString& InListName, const FOnReadFriendsListComplete& InDelegate);

	virtual void Initialize() override;
	virtual void Tick() override;
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

protected:

	virtual const FString GetTaskName() const override
	{
		return TEXT("FOnlineAsyncTaskAccelByteReadFriendsList");
	}

private:

	/**
	 * Name of the friends list that we wish to query, corresponds to EFriendsLists
	 */
	FString ListName;

	/** String representing an error message that is passed to the delegate, can be blank */
	FString ErrorString;

	/** Delegate that will be fired once we have finished our attempt to get the user's friends list */
	FOnReadFriendsListComplete Delegate;

	/** Whether we have gotten a response back from the backend for querying incoming friend requests */
	FThreadSafeBool bHasReceivedResponseForIncomingFriends;

	/** Whether we have gotten a response back from the backend for querying outgoing friend requests */
	FThreadSafeBool bHasReceivedResponseForOutgoingFriends;

	/** Whether we have gotten a response back from the backend for querying our current friends */
	FThreadSafeBool bHasReceivedResponseForCurrentFriends;

	/** Whether we have already sent a request to get information on all of our friends */
	FThreadSafeBool bHasSentRequestForFriendInformation;

	/** Whether we have gotten a response back from the backend for querying information on all of our friends */
	FThreadSafeBool bHasRecievedAllFriendInformation;

	/** Array of AccelByte IDs that we need to query from backend */
	TArray<FString> FriendIdsToQuery;

	/** Resulting array of friend instances from each query */
	TArray<TSharedPtr<FOnlineFriend>> FoundFriends;

	/** Map of AccelByte IDs to invite status, used to make final friend instance */
	TMap<FString, EInviteStatus::Type> AccelByteIdToFriendStatus;

	/** Convenience method for checking in tick whether the task is still waiting on async work from the backend */
	bool HasTaskFinishedAsyncWork();

	/** Delegate handler for when the friends list load has completed */
	void OnLoadFriendsListResponse(const FAccelByteModelsLoadFriendListResponse& Result);

	/** Delegate handler for when the incoming friend request load has completed */
	void OnListIncomingFriendsResponse(const FAccelByteModelsListIncomingFriendsResponse& Result);

	/** Delegate handler for when the outgoing friend request load has completed */
	void OnListOutgoingFriendsResponse(const FAccelByteModelsListOutgoingFriendsResponse& Result);

	/** Delegate handler for when we successfully get all information for each user in our friends list */
	void OnQueryFriendInformationComplete(bool bIsSuccessful, TArray<TSharedRef<FAccelByteUserInfo>> UsersQueried);

};

