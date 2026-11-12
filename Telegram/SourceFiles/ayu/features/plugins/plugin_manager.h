// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include <QtCore/QString>
#include <vector>
#include <memory>
#include <functional>

namespace Main {
class Session;
}

namespace Ayu::Plugins {

struct PluginMeta {
	QString id;
	QString name;
	QString description;
	QString author;
	QString version;
	QString filePath;
	bool enabled = true;
};

class PluginManager final {
public:
	static PluginManager &Instance();

	void init();
	void deinit();

	void rescanPlugins();
	bool loadPlugin(const QString &filePath);
	void unloadPlugin(const QString &pluginId);

	[[nodiscard]] const std::vector<PluginMeta> &plugins() const;
	void setPluginEnabled(const QString &pluginId, bool enabled);

	[[nodiscard]] QString pluginsDirectory() const;
	[[nodiscard]] bool isPythonAvailable() const;

	// Event hooks invoked from AyuGram Core
	void onMessageReceived(uint64 peerId, int msgId, const QString &text, uint64 senderId);
	bool onBeforeMessageSend(uint64 peerId, QString &text); // Returns false if canceled

private:
	PluginManager() = default;
	~PluginManager() = default;
	PluginManager(const PluginManager &) = delete;
	PluginManager &operator=(const PluginManager &) = delete;

	bool initPython();
	void deploySdkFiles();
	void registerInternalModule();

	bool _initialized = false;
	bool _pythonLoaded = false;
	QString _pluginsDir;
	QString _sdkDir;
	std::vector<PluginMeta> _plugins;
};

} // namespace Ayu::Plugins
