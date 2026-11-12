// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/plugins/plugin_manager.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>
#include <QtCore/QRegularExpression>
#include <QtCore/QTextStream>
#include <QtCore/QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace Ayu::Plugins {

namespace {

constexpr auto kBasePluginPy = R"(# exteraGram / AyuGram Python SDK Compatibility Layer for Desktop
from typing import Any, Optional, Dict, List

class HookResult:
    def __init__(self, new_args=None, return_value=None, cancel=False):
        self.new_args = new_args
        self.return_value = return_value
        self.cancel = cancel

class HookStrategy:
    BEFORE = 0
    AFTER = 1
    REPLACE = 2

class BasePlugin:
    def __init__(self):
        self.id = getattr(self.__class__, '__id__', getattr(self, '__id__', 'unknown_plugin'))
        self.name = getattr(self.__class__, '__name__', getattr(self, '__name__', self.id))
        self.description = getattr(self.__class__, '__description__', '')
        self.author = getattr(self.__class__, '__author__', 'Community')
        self.version = getattr(self.__class__, '__version__', '1.0.0')

    def on_load(self) -> None:
        pass

    def on_unload(self) -> None:
        pass

    def log(self, message: Any) -> None:
        print(f"[{self.name}] {message}")
)";

constexpr auto kClientUtilsPy = R"(# exteraGram / AyuGram client_utils for Desktop
from typing import Optional, Dict, Any, List

def send_message(peer_id: int, text: str, reply_to: Optional[int] = None) -> bool:
    try:
        import _ayugram_bridge
        return _ayugram_bridge.send_message(int(peer_id), str(text))
    except Exception as e:
        print(f"[client_utils] send_message error: {e}")
        return False

def get_current_chat() -> Optional[int]:
    try:
        import _ayugram_bridge
        return _ayugram_bridge.get_current_peer()
    except Exception:
        return None

def log(msg: Any) -> None:
    print(f"[AyuGram Desktop Plugin] {msg}")
)";

constexpr auto kHookUtilsPy = R"(# exteraGram / AyuGram hook_utils for Desktop
from base_plugin import HookResult, HookStrategy

_registered_hooks = []

def hook_method(target_class: str, method_name: str, callback=None, strategy=HookStrategy.BEFORE):
    global _registered_hooks
    _registered_hooks.append({
        'class': target_class,
        'method': method_name,
        'callback': callback,
        'strategy': strategy
    })
    print(f"[hook_utils] Hook registered on {target_class}.{method_name}")
)";

constexpr auto kUiPy = R"(# exteraGram / AyuGram ui module for Desktop
class SwitchItem:
    def __init__(self, key: str, title: str, default: bool = False, on_change = None):
        self.key = key
        self.title = title
        self.value = default
        self.on_change = on_change

class ButtonItem:
    def __init__(self, title: str, on_click = None):
        self.title = title
        self.on_click = on_click
)";

} // namespace

PluginManager &PluginManager::Instance() {
	static PluginManager instance;
	return instance;
}

void PluginManager::init() {
	if (_initialized) {
		return;
	}
	_initialized = true;

	const auto appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	_pluginsDir = appData + "/AyuGram/plugins";
	_sdkDir = appData + "/AyuGram/sdk";

	QDir().mkpath(_pluginsDir);
	QDir().mkpath(_sdkDir);

	deploySdkFiles();
	_pythonLoaded = initPython();

	if (_pythonLoaded) {
		registerInternalModule();
		rescanPlugins();
	}
}

void PluginManager::deinit() {
	_plugins.clear();
	_initialized = false;
	_pythonLoaded = false;
}

void PluginManager::deploySdkFiles() {
	auto writeIfMissing = [](const QString &path, const char *content) {
		QFile f(path);
		if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			QTextStream out(&f);
			out.setEncoding(QStringConverter::Utf8);
			out << content;
		}
	};

	writeIfMissing(_sdkDir + "/base_plugin.py", kBasePluginPy);
	writeIfMissing(_sdkDir + "/client_utils.py", kClientUtilsPy);
	writeIfMissing(_sdkDir + "/hook_utils.py", kHookUtilsPy);
	writeIfMissing(_sdkDir + "/ui.py", kUiPy);
}

bool PluginManager::initPython() {
#ifdef Q_OS_WIN
	// Check for common Python runtime DLLs dynamically
	const wchar_t *dllCandidates[] = {
		L"python311.dll",
		L"python312.dll",
		L"python310.dll",
		L"python3.dll"
	};

	for (const auto *dllName : dllCandidates) {
		HMODULE hMod = LoadLibraryW(dllName);
		if (hMod) {
			return true;
		}
	}
#endif
	return false;
}

void PluginManager::registerInternalModule() {
	// Bridge registration for Python scripts
}

void PluginManager::rescanPlugins() {
	_plugins.clear();
	QDir dir(_pluginsDir);
	const auto files = dir.entryInfoList({ "*.py" }, QDir::Files, QDir::Name);

	for (const auto &info : files) {
		loadPlugin(info.absoluteFilePath());
	}
}

bool PluginManager::loadPlugin(const QString &filePath) {
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly)) {
		return false;
	}

	QTextStream in(&file);
	in.setEncoding(QStringConverter::Utf8);
	const auto content = in.readAll();

	PluginMeta meta;
	meta.filePath = filePath;
	meta.id = QFileInfo(filePath).baseName();
	meta.name = meta.id;
	meta.version = "1.0.0";
	meta.author = "Unknown";
	meta.enabled = true;

	// Extract metadata from Python docstrings/globals: __id__, __name__, etc.
	const auto parseMeta = [&](const QString &key) -> QString {
		QRegularExpression re(QString(R"(__(?:%1)__\s*=\s*["']([^"']+)["'])").arg(key));
		auto match = re.match(content);
		return match.hasMatch() ? match.captured(1) : QString();
	};

	const auto extractedId = parseMeta("id");
	if (!extractedId.isEmpty()) meta.id = extractedId;

	const auto extractedName = parseMeta("name");
	if (!extractedName.isEmpty()) meta.name = extractedName;

	const auto extractedVer = parseMeta("version");
	if (!extractedVer.isEmpty()) meta.version = extractedVer;

	const auto extractedAuthor = parseMeta("author");
	if (!extractedAuthor.isEmpty()) meta.author = extractedAuthor;

	const auto extractedDesc = parseMeta("description");
	if (!extractedDesc.isEmpty()) meta.description = extractedDesc;

	_plugins.push_back(meta);
	return true;
}

void PluginManager::unloadPlugin(const QString &pluginId) {
	for (auto it = _plugins.begin(); it != _plugins.end(); ++it) {
		if (it->id == pluginId) {
			_plugins.erase(it);
			break;
		}
	}
}

const std::vector<PluginMeta> &PluginManager::plugins() const {
	return _plugins;
}

void PluginManager::setPluginEnabled(const QString &pluginId, bool enabled) {
	for (auto &p : _plugins) {
		if (p.id == pluginId) {
			p.enabled = enabled;
			break;
		}
	}
}

QString PluginManager::pluginsDirectory() const {
	return _pluginsDir;
}

bool PluginManager::isPythonAvailable() const {
	return _pythonLoaded;
}

void PluginManager::onMessageReceived(uint64 peerId, int msgId, const QString &text, uint64 senderId) {
	// Dispatch message event to active plugins
}

bool PluginManager::onBeforeMessageSend(uint64 peerId, QString &text) {
	// Dispatch outgoing message event to active plugins
	return true;
}

} // namespace Ayu::Plugins
