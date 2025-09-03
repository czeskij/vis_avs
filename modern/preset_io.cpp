#include "preset_io.h"
#include <cctype>
#include <fstream>
#include <sstream>

static std::string trim(const std::string& s)
{
    size_t a = 0;
    while (a < s.size() && std::isspace((unsigned char)s[a]))
        ++a;
    size_t b = s.size();
    while (b > a && std::isspace((unsigned char)s[b - 1]))
        --b;
    return s.substr(a, b - a);
}

bool save_oscstar_preset(const std::string& path, const OscStarParams& p)
{
    std::ofstream f(path);
    if (!f)
        return false;
    f << "{\n";
    f << "  \"arms\": " << p.arms << ",\n";
    f << "  \"baseSpeed\": " << p.baseSpeed << ",\n";
    f << "  \"levelSpeed\": " << p.levelSpeed << ",\n";
    f << "  \"trailFade\": " << p.trailFade << "\n";
    f << "}\n";
    return true;
}

std::optional<OscStarParams> load_oscstar_preset(const std::string& path)
{
    std::ifstream f(path);
    if (!f)
        return std::nullopt;
    std::stringstream ss;
    ss << f.rdbuf();
    std::string txt = ss.str();
    OscStarParams p;
    bool have = false;
    auto findNum = [&](const char* key, double& out) { size_t pos = txt.find(key); if(pos==std::string::npos) return false; pos = txt.find(':',pos); if(pos==std::string::npos) return false; size_t end=pos+1; while(end<txt.size() && (std::isspace((unsigned char)txt[end])||txt[end]==':')) ++end; size_t start=end; while(end<txt.size() && (std::isdigit((unsigned char)txt[end])||txt[end]=='-'||txt[end]=='+'||txt[end]=='.'||txt[end]=='e'||txt[end]=='E')) ++end; try { out = std::stod(txt.substr(start,end-start)); return true; } catch(...){ return false; } };
    double v;
    if (findNum("\"arms\"", v)) {
        p.arms = (int)v;
        have = true;
    }
    if (findNum("\"baseSpeed\"", v)) {
        p.baseSpeed = (float)v;
        have = true;
    }
    if (findNum("\"levelSpeed\"", v)) {
        p.levelSpeed = (float)v;
        have = true;
    }
    if (findNum("\"trailFade\"", v)) {
        p.trailFade = (float)v;
        have = true;
    }
    if (!have)
        return std::nullopt;
    return p;
}
