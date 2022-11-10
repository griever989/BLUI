#include "RenderHandler.h"
#include "Interfaces/IPluginManager.h"
#include "BluEye.h"

void BrowserClient::OnAfterCreated(CefRefPtr<CefBrowser> Browser)
{
	//CEF_REQUIRE_UI_THREAD();
	if (!BrowserRef.get())
	{
		// Keep a reference to the main browser.
		BrowserRef = Browser;
		BrowserId = Browser->GetIdentifier();
	}
}

void BrowserClient::OnBeforeClose(CefRefPtr<CefBrowser> Browser)
{
	//CEF_REQUIRE_UI_THREAD();
	if (BrowserId == Browser->GetIdentifier())
	{
		BrowserRef.reset();
	}
}

bool BrowserClient::OnConsoleMessage(CefRefPtr<CefBrowser> Browser, cef_log_severity_t Level, const CefString& Message, const CefString& source, int line)
{
	FString LogMessage = FString(Message.c_str());
	LogEmitter->Broadcast(LogMessage);
	return true;
}

void BrowserClient::OnFullscreenModeChange(CefRefPtr< CefBrowser > Browser, bool Fullscreen)
{
	UE_LOG(LogTemp, Log, TEXT("Changed to Fullscreen: %d"), Fullscreen);
}

void BrowserClient::OnTitleChange(CefRefPtr< CefBrowser > Browser, const CefString& Title)
{
	FString TitleMessage = FString(Title.c_str());
	LogEmitter->Broadcast(TitleMessage);
}

CefRefPtr<CefBrowser> BrowserClient::GetCEFBrowser()
{
	return BrowserRef;
}

bool BrowserClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, CefProcessId SourceProcess, CefRefPtr<CefProcessMessage> Message)
{
	FString Data;
	FString Name = FString(UTF8_TO_TCHAR(Message->GetArgumentList()->GetString(0).ToString().c_str()));
	FString Type = FString(UTF8_TO_TCHAR(Message->GetArgumentList()->GetString(2).ToString().c_str()));
	FString DataType = FString(UTF8_TO_TCHAR(Message->GetArgumentList()->GetString(3).ToString().c_str()));

	if (Type == "js_event")
	{

		// Check the datatype

		if (DataType == "bool")
			Data = Message->GetArgumentList()->GetBool(1) ? TEXT("true") : TEXT("false");
		else if (DataType == "int")
			Data = FString::FromInt(Message->GetArgumentList()->GetInt(1));
		else if (DataType == "string")
			Data = FString(UTF8_TO_TCHAR(Message->GetArgumentList()->GetString(1).ToString().c_str()));
		else if (DataType == "double")
			Data = FString::SanitizeFloat(Message->GetArgumentList()->GetDouble(1));

		EventEmitter->Broadcast(Name, Data);
	}

	return true;
}

void BrowserClient::OnUncaughtException(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, CefRefPtr<CefV8Context> Context, CefRefPtr<CefV8Exception> Exception, CefRefPtr<CefV8StackTrace> StackTrace)
{
	FString ErrorMessage = FString(Exception->GetMessage().c_str());
	UE_LOG(LogClass, Warning, TEXT("%s"), *ErrorMessage);
}

//The path slashes have to be reversed to work with CEF
FString ReversePathSlashes(FString ForwardPath)
{
	return ForwardPath.Replace(TEXT("/"), TEXT("\\"));
}
FString UtilityBLUIDownloadsFolder()
{
	return ReversePathSlashes(FPaths::ConvertRelativePathToFull(IPluginManager::Get().FindPlugin("BLUI")->GetBaseDir() + "/Downloads/"));
}


void BrowserClient::SetEventEmitter(FScriptEvent* Emitter)
{
	this->EventEmitter = Emitter;
}

void BrowserClient::SetLogEmitter(FLogEvent* Emitter)
{
	this->LogEmitter = Emitter;
}

void BrowserClient::OnBeforeDownload(
	CefRefPtr<CefBrowser> Browser,
	CefRefPtr<CefDownloadItem> DownloadItem,
	const CefString & SuggestedName,
	CefRefPtr<CefBeforeDownloadCallback> Callback)
{
	UNREFERENCED_PARAMETER(Browser);
	UNREFERENCED_PARAMETER(DownloadItem);

	//We use this concatenation method to mix c_str with regular FString and then convert the result back to c_str
	FString DownloadPath = UtilityBLUIDownloadsFolder() + FString(SuggestedName.c_str());

	Callback->Continue(*DownloadPath, false);	//don't show the download dialog, just go for it

	UE_LOG(LogClass, Log, TEXT("Downloading file for path %s"), *DownloadPath);
}

void BrowserClient::OnDownloadUpdated(
	CefRefPtr<CefBrowser> ForBrowser,
	CefRefPtr<CefDownloadItem> DownloadItem,
	CefRefPtr<CefDownloadItemCallback> Callback)
{
	int Percentage = DownloadItem->GetPercentComplete();
	FString Url = FString(DownloadItem->GetFullPath().c_str());
	
	UE_LOG(LogClass, Log, TEXT("Download %s Updated: %d"), *Url , Percentage);

	if (Percentage == 100 && DownloadItem->IsComplete()) {
		UE_LOG(LogClass, Log, TEXT("Download %s Complete"), *Url);
	}

	//Example download cancel/pause etc, we just have to hijack this
	//callback->Cancel();
}

