#include "stdafx.h"

AsiSupport::AsiLoader::AsiLoader(String^ workingDir)
{
	instance = this;

	this->WorkingDir = workingDir;
	this->Loading = false;
	this->LoadedPlugins = gcnew List<Plugin^>();
}

void AsiSupport::AsiLoader::Initialize()
{
	Log::Info("Loading ASI plugins");

	for each(String^ file in Directory::EnumerateFiles(this->WorkingDir, "*.asi"))
	{
		LoadPlugin(file);
		Rage::GameFiber::Sleep(0);
	}

	Log::Info("Finished loading ASI plugins");
}

bool AsiSupport::AsiLoader::IsLoaded(String^ path)
{
	for each(Plugin^ plugin in this->LoadedPlugins)
	{
		if(plugin->FileName->Equals(path))
			return true;
	}

	return false;
}

void AsiSupport::AsiLoader::LoadPlugin(String^ path)
{
	this->Loading = true;
	const string name = Log::ToUnmanaged(Path::GetFileNameWithoutExtension(path));

	if(this->IsLoaded(path))
		Log::Info("Plugin " + name + " already loaded.");
	else
	{
		const string pluginPath = Log::ToUnmanaged(path);

		Log::Info("Loading \"" + name + '"');

		PluginImage pluginImage;

		if(!pluginImage.Load(pluginPath))
		{
			Log::Error("Unable to load \"" + name + "\" image.");
			this->Loading = false;
			return;
		}

		Rage::GameFiber::Sleep(0);

		if(!pluginImage.IsCompatible())
		{
			Log::Info("Detected ScriptHookV image. Patching it...");

			if(pluginImage.PatchCompatibility())
			{
				Log::Info("Successfully patched \"" + name + '"');
			}
			else
			{
				Log::Error("Failed to patch \"" + name + '"');
				this->Loading = false;
				return;
			}
		}

		Rage::GameFiber::Sleep(0);

		//At this point the plugin should be compatible

		Plugin^ currentPlugin = gcnew Plugin(pluginPath);
		this->LoadedPlugins->Add(currentPlugin);
		HMODULE module = LoadLibraryA(pluginPath.c_str());

		if(module)
		{
			Log::Info("Plugin \"" + name + "\" loaded successfully.");

			for each(PluginThread^ thread in currentPlugin->ScriptThreads)
				thread->Start();
		}
		else
		{
			this->LoadedPlugins->Remove(currentPlugin);
			Log::Error("Unable to load \"" + name + "\" (error code " + to_string(GetLastError()) + ")");
		}
	}

	this->Loading = false;
}

AsiSupport::Plugin^ AsiSupport::AsiLoader::GetPlugin(String ^ name)
{
	for each(Plugin^ plugin in this->LoadedPlugins)
	{
		if(Path::GetFileNameWithoutExtension(plugin->FileName)->Equals(name))
			return plugin;
	}

	return nullptr;
}

AsiSupport::Plugin^ AsiSupport::AsiLoader::GetPlugin(HMODULE pluginModule)
{
	String^ pluginPath;
	char pluginPathArray[MAX_PATH];
	GetModuleFileNameA(pluginModule, pluginPathArray, MAX_PATH);
	pluginPath = gcnew String(pluginPathArray);

	for each(Plugin^ plugin in this->LoadedPlugins)
	{
		if(pluginPath->Equals(plugin->FileName))
			return plugin;
	}

	return nullptr;
}

void AsiSupport::AsiLoader::UnloadPlugin(Plugin^ plugin)
{
	string name = Log::ToUnmanaged(Path::GetFileNameWithoutExtension(plugin->FileName));

	Log::Info("Unloading \"" + name +'"');

	for each(PluginThread^ thread in plugin->ScriptThreads)
		thread->Fiber->Abort();

	string fileName = Log::ToUnmanaged(plugin->FileName);

	this->LoadedPlugins->Remove(plugin);

	if(FreeLibrary(GetModuleHandleA(fileName.c_str())))
		Log::Info("Plugin \"" + name + "\" unloaded successfully.");
	else
		Log::Info("Unable to unload plugin \"" + name + "\" (error code " + to_string(GetLastError()) + ")");
}

void AsiSupport::AsiLoader::UnloadAllPlugins()
{
	List<Plugin^>^ plugins = gcnew List<Plugin^>(this->LoadedPlugins);

	for each(Plugin^ plugin in plugins)
		this->UnloadPlugin(plugin);
}
