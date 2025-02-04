﻿// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineAsyncTaskAccelByteLoginServer.h"
#include "GameServerApi/AccelByteServerOauth2Api.h"
#include "OnlineIdentityInterfaceAccelByte.h"

FOnlineAsyncTaskAccelByteLoginServer::FOnlineAsyncTaskAccelByteLoginServer(FOnlineSubsystemAccelByte* const InABInterface, int32 InLocalUserNum)
    : FOnlineAsyncTaskAccelByte(InABInterface, ASYNC_TASK_FLAG_BIT(EAccelByteAsyncTaskFlags::ServerTask))
{
    LocalUserNum = InLocalUserNum;
}

void FOnlineAsyncTaskAccelByteLoginServer::Initialize()
{
    Super::Initialize();

    AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));

	const FVoidHandler OnServerLoginSuccessDelegate = FVoidHandler::CreateRaw(this, &FOnlineAsyncTaskAccelByteLoginServer::OnLoginServerSuccess);
	const FErrorHandler OnServerLoginErrorDelegate = FErrorHandler::CreateRaw(this, &FOnlineAsyncTaskAccelByteLoginServer::OnLoginServerError);
	FRegistry::ServerOauth2.LoginWithClientCredentials(OnServerLoginSuccessDelegate, OnServerLoginErrorDelegate);

    AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteLoginServer::Finalize()
{
    AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("bWasSuccessful: %s"), LOG_BOOL_FORMAT(bWasSuccessful));

    if (bWasSuccessful)
    {
		const FOnlineIdentityAccelBytePtr IdentityInterface = StaticCastSharedPtr<FOnlineIdentityAccelByte>(Subsystem->GetIdentityInterface());
		if (!ensure(IdentityInterface.IsValid()))
        {
            AB_OSS_ASYNC_TASK_TRACE_END_VERBOSITY(Warning, TEXT("Failed to finalize server login as our identity interface is invalid!"));
            return;
        }

        IdentityInterface->AddAuthenticatedServer(LocalUserNum);
    }

    AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteLoginServer::TriggerDelegates()
{
    AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("bWasSuccessful: %s"), LOG_BOOL_FORMAT(bWasSuccessful));
	
    const FOnlineIdentityAccelBytePtr IdentityInterface = StaticCastSharedPtr<FOnlineIdentityAccelByte>(Subsystem->GetIdentityInterface());
	if (!ensure(IdentityInterface.IsValid()))
	{
		AB_OSS_ASYNC_TASK_TRACE_END_VERBOSITY(Warning, TEXT("Failed to trigger delegates for server login as our identity interface is invalid!"));
		return;
	}

    // #NOTE Deliberately passing an invalid user ID here as server's do not have user IDs. This shouldn't cause any issues for auto login.
	IdentityInterface->TriggerOnLoginCompleteDelegates(LocalUserNum, bWasSuccessful, FUniqueNetIdAccelByteUser::Invalid().Get(), ErrorString);

    AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteLoginServer::OnLoginServerSuccess()
{
    AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));

    CompleteTask(EAccelByteAsyncTaskCompleteState::Success);

    AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteLoginServer::OnLoginServerError(int32 ErrorCode, const FString& ErrorMessage)
{
    UE_LOG_AB(Warning, TEXT("Failed to authenticate server with AccelByte! Error code: %d; Error message: %s"), ErrorCode, *ErrorMessage);
    CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);
    ErrorString = TEXT("server-login-failed");
}
