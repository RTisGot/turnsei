#include "AIDialogue.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>

#pragma comment(lib, "winhttp.lib")

namespace {

// JSON文字列エスケープ
std::string EscapeJson(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 16);
    for (unsigned char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (c < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                out += buf;
            } else {
                out += (char)c;
            }
        }
    }
    return out;
}

// システムプロンプト + リクエストJSONを構築
std::string BuildRequestBody(
    const std::string& playerInput,
    const CharacterPersonality& persona,
    const DialogueContext& ctx)
{
    std::string sys;
    sys += "あなたはターン制RPGのキャラクター「" + persona.name + "」です。\n";
    sys += "役割: " + persona.role + "\n";
    sys += "性格・口調: " + persona.traits + "\n";
    sys += "話し方: " + persona.speechStyle + "\n";
    if (!persona.catchphrase.empty())
        sys += "口癖: " + persona.catchphrase + "\n";

    sys += "\n【現在の状況】\n";
    if (ctx.inBattle) {
        sys += "・戦闘中。相手: " + (ctx.enemyName.empty() ? "謎の敵" : ctx.enemyName) + "\n";
        sys += "・自分のHP: " + std::to_string(ctx.playerHp) + "/" + std::to_string(ctx.playerMaxHp) + "\n";
        sys += "・敵のHP: " + std::to_string(ctx.enemyHp) + "/" + std::to_string(ctx.enemyMaxHp) + "\n";
        sys += "・ターン数: " + std::to_string(ctx.turnNumber) + "\n";
        sys += "・" + std::string(ctx.isPlayerTurn ? "今は自分のターン" : "敵のターン待ち") + "\n";
    } else {
        sys += "・フィールドを移動中（非戦闘時）\n";
    }

    sys += "\nキャラクターとして返答してください。返答は1〜2文程度で簡潔に。キャラクターを崩さないこと。";

    std::string body;
    body += "{\"model\":\"claude-opus-4-8\",";
    body += "\"max_tokens\":300,";
    body += "\"system\":\"" + EscapeJson(sys) + "\",";
    body += "\"messages\":[{\"role\":\"user\",\"content\":\"" + EscapeJson(playerInput) + "\"}]}";
    return body;
}

// JSONレスポンスからtextフィールドを抽出
std::string ExtractText(const std::string& json)
{
    const std::string key = "\"text\":\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos) return "";
    pos += key.size();

    std::string result;
    bool escaped = false;
    for (size_t i = pos; i < json.size(); ++i) {
        char c = json[i];
        if (escaped) {
            switch (c) {
            case '"':  result += '"';  break;
            case '\\': result += '\\'; break;
            case 'n':  result += '\n'; break;
            case 'r':  result += '\r'; break;
            case 't':  result += '\t'; break;
            default:   result += c;   break;
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            break;
        } else {
            result += c;
        }
    }
    return result;
}

// WinHTTPでPOSTリクエスト送信
std::string HttpPost(const std::string& body, const std::string& apiKey)
{
    std::wstring wApiKey(apiKey.begin(), apiKey.end());

    HINTERNET hSession = WinHttpOpen(
        L"TurnseiBattleAI/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(
        hSession, L"api.anthropic.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"POST", L"/v1/messages",
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::wstring headers =
        L"Content-Type: application/json\r\n"
        L"anthropic-version: 2023-06-01\r\n"
        L"x-api-key: " + wApiKey + L"\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL sent = WinHttpSendRequest(
        hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)body.c_str(), (DWORD)body.size(),
        (DWORD)body.size(), 0);

    std::string result;
    if (sent && WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD available = 0;
        while (WinHttpQueryDataAvailable(hRequest, &available) && available > 0) {
            std::string chunk(available, '\0');
            DWORD read = 0;
            WinHttpReadData(hRequest, &chunk[0], available, &read);
            chunk.resize(read);
            result += chunk;
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

} // namespace

std::string CallClaudeDialogue(
    const std::string& playerInput,
    const CharacterPersonality& personality,
    const DialogueContext& context)
{
    char* key = nullptr;
    size_t keyLen = 0;
    _dupenv_s(&key, &keyLen, "ANTHROPIC_API_KEY");
    if (!key || keyLen == 0) {
        free(key);
        return "[APIキーが設定されていません\n環境変数 ANTHROPIC_API_KEY を設定してください]";
    }

    std::string body = BuildRequestBody(playerInput, personality, context);
    std::string apiKeyStr(key);
    free(key);
    std::string response = HttpPost(body, apiKeyStr);

    if (response.empty()) return "[接続に失敗しました]";

    std::string text = ExtractText(response);
    return text.empty() ? "[応答の解析に失敗しました]" : text;
}

void CallClaudeDialogueAsync(
    const std::string& playerInput,
    const CharacterPersonality& personality,
    const DialogueContext& context,
    DialogueCallback callback)
{
    std::thread([=]() {
        std::string result = CallClaudeDialogue(playerInput, personality, context);
        callback(result);
    }).detach();
}

CharacterPersonality GetPersonalityForCharacter(const std::string& characterName)
{
    // キャラクター名ごとに性格を定義。追加・変更自由。
    if (characterName == "Hero" || characterName == "Yuki" || characterName == "勇者") {
        return { characterName, "warrior", "casual", "勇敢で前向き。仲間を大切にする。困難な状況でも諦めない熱血漢。", "行くぞ！" };
    }
    if (characterName == "Mia" || characterName == "Aria") {
        return { characterName, "mage", "polite", "知識豊富で冷静。魔法の研究に熱心。ちょっと抜けているところもある。", "なるほど..." };
    }
    if (characterName == "Rex" || characterName == "Garth") {
        return { characterName, "warrior", "gruff", "無口で頼りになる歴戦の戦士。言葉は少ないが義理人情に厚い。", "..." };
    }
    // デフォルト
    return { characterName, "adventurer", "casual", "冒険者として旅をしている。状況に応じて柔軟に対応する。", "" };
}
