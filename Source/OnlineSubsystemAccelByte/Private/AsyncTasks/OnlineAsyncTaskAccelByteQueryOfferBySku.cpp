﻿// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineAsyncTaskAccelByteQueryOfferBySku.h"

FOnlineAsyncTaskAccelByteQueryOfferBySku::FOnlineAsyncTaskAccelByteQueryOfferBySku(
	FOnlineSubsystemAccelByte* const InABSubsystem, const FUniqueNetId& InUserId, const FString& InSku,
	const FOnQueryOnlineStoreOffersComplete& InDelegate) 
	: FOnlineAsyncTaskAccelByte(InABSubsystem)
	, Sku(InSku)
	, Delegate(InDelegate)
	, Offer(MakeShared<FOnlineStoreOffer>())
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));

	UserId = StaticCastSharedRef<const FUniqueNetIdAccelByteUser>(InUserId.AsShared());
	
	Language = Subsystem->GetLanguage();
	
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteQueryOfferBySku::Initialize()
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("Initialized"));
	Super::Initialize();
	
	OnSuccess = THandler<FAccelByteModelsItemInfo>::CreateRaw(this, &FOnlineAsyncTaskAccelByteQueryOfferBySku::HandleGetItemBySku);
	OnError = FErrorHandler::CreateRaw(this, &FOnlineAsyncTaskAccelByteQueryOfferBySku::HandleAsyncTaskError);
	ApiClient->Item.GetItemBySku(Sku, Language, TEXT(""), OnSuccess, OnError);
	
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteQueryOfferBySku::Finalize()
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("Finalized"));
	Super::Finalize();
	
	const FOnlineStoreV2AccelBytePtr StoreV2Interface = StaticCastSharedPtr<FOnlineStoreV2AccelByte>(Subsystem->GetStoreV2Interface());
	StoreV2Interface->EmplaceOffers(TMap<FUniqueOfferId, FOnlineStoreOfferRef>{TPair<FUniqueOfferId, FOnlineStoreOfferRef>{Offer->OfferId, Offer}});
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteQueryOfferBySku::TriggerDelegates()
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("Trigger Delegates"));
	Super::TriggerDelegates();

	TArray<FString> QueriedOfferIds{Offer->OfferId};
	Delegate.ExecuteIfBound(bWasSuccessful, QueriedOfferIds, ErrorMsg);
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteQueryOfferBySku::HandleGetItemBySku(const FAccelByteModelsItemInfo& Result)
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("Get Success"));
	Offer->OfferId = Result.ItemId;
	Offer->NumericPrice = Result.RegionData[0].DiscountedPrice;
	Offer->RegularPrice = Result.RegionData[0].Price;
	Offer->CurrencyCode = Result.RegionData[0].CurrencyCode;
	Offer->Title = FText::FromString(Result.Title);
	if (Result.Images.Num() > 0)
	{
		Offer->DynamicFields.Add(TEXT("IconUrl"), Result.Images[0].ImageUrl);
	}
	Offer->DynamicFields.Add(TEXT("Region"), Result.Region);
	Offer->DynamicFields.Add(TEXT("IsConsumable"), Result.EntitlementType == EAccelByteEntitlementType::CONSUMABLE ? TEXT("true") : TEXT("false"));
	Offer->DynamicFields.Add(TEXT("Category"), Result.CategoryPath);
	Offer->DynamicFields.Add(TEXT("Name"), Result.Name);
	Offer->DynamicFields.Add(TEXT("ItemType"), FAccelByteUtilities::GetUEnumValueAsString(Result.ItemType));
	Offer->DynamicFields.Add(TEXT("Sku"), Result.Sku);
	if (Result.ItemType == EAccelByteItemType::COINS)
	{
		Offer->DynamicFields.Add(TEXT("TargetCurrencyCode"), Result.TargetCurrencyCode);
	}
	CompleteTask(EAccelByteAsyncTaskCompleteState::Success);
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteQueryOfferBySku::HandleAsyncTaskError(int32 Code, FString const& ErrMsg)
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN_VERBOSITY(Error, TEXT("Code: %d; Message: %s"), Code, *ErrMsg);
	
	ErrorMsg = ErrMsg;
	CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}