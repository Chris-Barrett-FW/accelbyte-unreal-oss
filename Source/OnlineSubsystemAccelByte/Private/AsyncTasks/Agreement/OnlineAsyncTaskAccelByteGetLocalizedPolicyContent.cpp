// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineAsyncTaskAccelByteGetLocalizedPolicyContent.h"
#include "OnlineSubsystemAccelByte.h"
#include "OnlineIdentityInterfaceAccelByte.h"
#include "OnlineAgreementInterfaceAccelByte.h"

FOnlineAsyncTaskAccelByteGetLocalizedPolicyContent::FOnlineAsyncTaskAccelByteGetLocalizedPolicyContent(FOnlineSubsystemAccelByte* const InABInterface, const FUniqueNetId& InLocalUserId, const FString& InBasePolicyId, const FString& InLocaleCode, bool bInAlwaysRequestToService)
	: FOnlineAsyncTaskAccelByte(InABInterface)
	, BasePolicyId(InBasePolicyId)
	, LocaleCode(InLocaleCode)
	, bAlwaysRequestToService(bInAlwaysRequestToService)
{
	UserId = StaticCastSharedRef<const FUniqueNetIdAccelByteUser>(InLocalUserId.AsShared());
}

void FOnlineAsyncTaskAccelByteGetLocalizedPolicyContent::Initialize()
{
	Super::Initialize();

	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("UserId: %s, BasePolicyId: %s"), *UserId->ToDebugString(), *BasePolicyId);

	const FOnlineAgreementAccelBytePtr AgreementInterface = Subsystem->GetAgreementInterface();
	if (AgreementInterface.IsValid())
	{
		FString InLocalizedPolicyContent;
		if (AgreementInterface->GetLocalizedPolicyContentFromCache(BasePolicyId, LocaleCode, InLocalizedPolicyContent) && !bAlwaysRequestToService)
		{
			LocalizedPolicyContent = InLocalizedPolicyContent;
			CompleteTask(EAccelByteAsyncTaskCompleteState::Success);
			AB_OSS_ASYNC_TASK_TRACE_END(TEXT("Localized Policy Content found for user index '%d'!"), LocalUserNum);
		}
		else
		{
			TArray<TSharedRef<FAccelByteModelsRetrieveUserEligibilitiesResponse>> EligibilitiesRef;
			if (AgreementInterface->GetEligibleAgreements(LocalUserNum, EligibilitiesRef))
			{
				bool bNotFound = false;
				FString DefaultPolicyLocation = TEXT("");
				for (const auto& Eligibility : EligibilitiesRef)
				{
					if (Eligibility->PolicyId == BasePolicyId)
					{
						BaseUrl = Eligibility->BaseUrls[0];
						for (const auto& Version : Eligibility->PolicyVersions)
						{
							if (Version.IsInEffect)
							{
								for (const auto& Localized : Version.LocalizedPolicyVersions)
								{
									if (Localized.LocaleCode == LocaleCode)
									{
										AttachmentLocation = Localized.AttachmentLocation;
										break;
									}
									if (Localized.IsDefaultSelection)
									{
										DefaultPolicyLocation = Localized.AttachmentLocation;
									}
								}
								if (AttachmentLocation.IsEmpty())
								{
									if (DefaultPolicyLocation.IsEmpty())
									{
										bNotFound = true;
									}
									else
									{
										AttachmentLocation = DefaultPolicyLocation;
									}
								}
							}
							if (!AttachmentLocation.IsEmpty())
							{
								break;
							}
							if (bNotFound)
							{
								break;
							}
						}
					}
					if (!BaseUrl.IsEmpty())
					{
						break;
					}
					if (bNotFound)
					{
						break;
					}
				}
				if (bNotFound)
				{
					ErrorStr = TEXT("request-failed-localized-content-not-found-error");
					UE_LOG_AB(Warning, TEXT("Failed to get localized policy content! Localized Content Not Found"));
					CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);
				}
				else
				{
					// Create delegates for successfully as well as unsuccessfully get localized policy content
					OnGetLocalizedPolicyContentSuccessDelegate = TDelegateUtils<THandler<FString>>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteGetLocalizedPolicyContent::OnGetLocalizedPolicyContentSuccess);
					OnGetLocalizedPolicyContentErrorDelegate = TDelegateUtils<FErrorHandler>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteGetLocalizedPolicyContent::OnGetLocalizedPolicyContentError);

					if (!BaseUrl.EndsWith("/"))
					{
						BaseUrl += "/";
					}

					// Send off a request to get localized policy content, as well as connect our delegates for doing so
					ApiClient->Agreement.GetLegalDocument(FString::Printf(TEXT("%s%s"), *BaseUrl, *AttachmentLocation), OnGetLocalizedPolicyContentSuccessDelegate, OnGetLocalizedPolicyContentErrorDelegate);
				}
			}
			else
			{
				ErrorStr = TEXT("request-failed-eligible-agreement-not-found-error");
				UE_LOG_AB(Warning, TEXT("Failed to get localized policy content! Eligible Agreements Not Found, please query eligible agreements first"));
				CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);
			}
		}
	}

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteGetLocalizedPolicyContent::TriggerDelegates()
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("bWasSuccessful: %s"), LOG_BOOL_FORMAT(bWasSuccessful));

	const FOnlineAgreementAccelBytePtr AgreementInterface = Subsystem->GetAgreementInterface();
	if (AgreementInterface.IsValid())
	{
		if (bWasSuccessful)
		{
			AgreementInterface->TriggerOnGetLocalizedPolicyContentCompletedDelegates(LocalUserNum, true, LocalizedPolicyContent, TEXT(""));
		}
		else
		{
			AgreementInterface->TriggerOnGetLocalizedPolicyContentCompletedDelegates(LocalUserNum, false, TEXT(""), ErrorStr);
		}
	}

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteGetLocalizedPolicyContent::OnGetLocalizedPolicyContentSuccess(const FString& Result)
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));

	const FOnlineAgreementAccelBytePtr AgreementInterface = Subsystem->GetAgreementInterface();
	LocalizedPolicyContent = Result;

	if (AgreementInterface.IsValid())
	{	
		auto ExistingLocalizedPolicies = AgreementInterface->LocalizedContentMap.Find(BasePolicyId);
		if (ExistingLocalizedPolicies == nullptr)
		{
			TArray<FABLocalizedPolicyContent> LocalizedPolicies{ FABLocalizedPolicyContent{LocalizedPolicyContent, LocaleCode} };
			AgreementInterface->LocalizedContentMap.Emplace(BasePolicyId, LocalizedPolicies);
		}
		else
		{
			ExistingLocalizedPolicies->Add(FABLocalizedPolicyContent{LocalizedPolicyContent, LocaleCode});
			AgreementInterface->LocalizedContentMap.Emplace(BasePolicyId, *ExistingLocalizedPolicies);
		}
	}

	CompleteTask(EAccelByteAsyncTaskCompleteState::Success);
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT("Sending request to get localized policy content for user '%s'!"), *UserId->ToDebugString());
}

void FOnlineAsyncTaskAccelByteGetLocalizedPolicyContent::OnGetLocalizedPolicyContentError(int32 ErrorCode, const FString& ErrorMessage)
{
	ErrorStr = TEXT("request-failed-get-localized-content-error");
	UE_LOG_AB(Warning, TEXT("Failed to get localized policy content! Error Code: %d; Error Message: %s"), ErrorCode, *ErrorMessage);
	CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);
}