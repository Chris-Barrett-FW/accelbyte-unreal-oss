﻿// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineAsyncTaskAccelByteQueryChildCategories.h"


FOnlineAsyncTaskAccelByteQueryChildCategories::FOnlineAsyncTaskAccelByteQueryChildCategories(
	FOnlineSubsystemAccelByte* const InABSubsystem, const FUniqueNetId& InUserId, const FString& InCategoryPath, const FOnQueryOnlineStoreCategoriesComplete& InDelegate) 
	: FOnlineAsyncTaskAccelByte(InABSubsystem, true)
	, CategoryPath(InCategoryPath)
	, Delegate(InDelegate)
	, ErrorMsg(TEXT(""))
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));

	UserId = StaticCastSharedRef<const FUniqueNetIdAccelByteUser>(InUserId.AsShared());
	Language = Subsystem->GetLanguage();
	
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteQueryChildCategories::Initialize()
{
	Super::Initialize();
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));
	
	const FOnlineStoreV2AccelBytePtr StoreV2Interface = StaticCastSharedPtr<FOnlineStoreV2AccelByte>(Subsystem->GetStoreV2Interface());
	FOnlineStoreCategory RootCategory;
	StoreV2Interface->GetCategory(CategoryPath, RootCategory);
	RootCategory.SubCategories.Reset();
	CategoryMap.Add(RootCategory.Id, RootCategory);

	OnGetChildCategoriesSuccess = TDelegateUtils<THandler<TArray<FAccelByteModelsCategoryInfo>>>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteQueryChildCategories::HandleGetChildCategoriesSuccess);
	OnError = TDelegateUtils<FErrorHandler>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteQueryChildCategories::HandleAsyncTaskError);
	
	ApiClient->Category.GetDescendantCategories(Language, CategoryPath,OnGetChildCategoriesSuccess, OnError);
	
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteQueryChildCategories::Finalize()
{
	Super::Finalize();
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));

	const FOnlineStoreV2AccelBytePtr StoreV2Interface = StaticCastSharedPtr<FOnlineStoreV2AccelByte>(Subsystem->GetStoreV2Interface());
	TArray<FOnlineStoreCategory> Categories;
	CategoryMap.GenerateValueArray(Categories);
	StoreV2Interface->EmplaceCategories(Categories);
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteQueryChildCategories::TriggerDelegates()
{
	Super::TriggerDelegates();
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));

	Delegate.ExecuteIfBound(bWasSuccessful, ErrorMsg);
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteQueryChildCategories::HandleGetChildCategoriesSuccess(const TArray<FAccelByteModelsCategoryInfo>& AccelByteModelsCategoryInfos)
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));
	for(const FAccelByteModelsCategoryInfo& CategoryInfo : AccelByteModelsCategoryInfos)
	{
		FOnlineStoreCategory& Category = CategoryMap.FindOrAdd(CategoryInfo.ParentCategoryPath);
		Category.Id = CategoryInfo.ParentCategoryPath;
		FOnlineStoreCategory Child;
		Child.Id = CategoryInfo.CategoryPath;
		Child.Description = FText::FromString(CategoryInfo.DisplayName);
		Category.SubCategories.Add(Child);
		CategoryMap.Add(Child.Id, Child);
	}
	CompleteTask(EAccelByteAsyncTaskCompleteState::Success);
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteQueryChildCategories::HandleAsyncTaskError(int32 Code, FString const& ErrMsg)
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN_VERBOSITY(Error, TEXT("Code: %d; Message: %s"), Code, *ErrMsg);

	ErrorMsg = ErrMsg;
	CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}
