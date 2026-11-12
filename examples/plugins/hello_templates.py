# AyuGram & exteraGram compatible Python Plugin
from base_plugin import BasePlugin, HookResult
import client_utils

__id__ = "templates_desktop"
__name__ = "Templates"
__description__ = "Быстрые шаблоны сообщений для AyuGram Desktop"
__author__ = "mr-Vestr & RimeHold"
__version__ = "1.0.0"

class TemplatesPlugin(BasePlugin):
    def on_load(self):
        self.log("Плагин Templates успешно загружен в AyuGram Desktop!")
        self.templates = {
            ".hi": "Привет! Как дела?",
            ".shrug": "¯\\_(ツ)_/¯",
            ".rules": "Пожалуйста, соблюдайте правила сервера RimeHold!"
        }

    def on_unload(self):
        self.log("Плагин Templates выгружен.")

    def on_send_message(self, peer_id: int, text: str):
        # Автоматическая замена шаблонов перед отправкой
        if text in self.templates:
            return HookResult(new_args=(peer_id, self.templates[text]))
        return HookResult()
