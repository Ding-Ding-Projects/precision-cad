#pragma once
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
namespace precision::preferences {
class JsonShapeScanner {
public:
    explicit JsonShapeScanner(const QByteArray &bytes) : b(bytes) {}
    bool invalid() { ws(); return !value(0) || (ws(), p != b.size()); }
private:
    const QByteArray &b; qsizetype p=0;
    void ws(){ while(p<b.size() && QByteArray(" \t\r\n").contains(b[p])) ++p; }
    bool token(QByteArray *raw=nullptr) { ws(); if(p>=b.size()||b[p]!='\"') return false; const auto start=p++; bool esc=false; while(p<b.size()){ const char c=b[p++]; if(esc){ esc=false; continue; } if(c=='\\') {esc=true; continue;} if(c=='\"'){if(raw)*raw=b.mid(start,p-start); return true;} } return false; }
    bool value(int depth) { ws(); if(p>=b.size()) return false; if(b[p]=='{') return object(depth+1); if(b[p]=='[') return array(depth+1); if(b[p]=='\"') return token(); const auto start=p; while(p<b.size()&&!QByteArray(" \t\r\n,]}").contains(b[p])) ++p; return p>start; }
    bool object(int depth) { if(depth>4 || b[p++]!='{') return false; QSet<QString> seen; ws(); if(p<b.size()&&b[p]=='}'){++p;return true;} while(true){ QByteArray raw; if(!token(&raw))return false; QJsonParseError e; const auto d=QJsonDocument::fromJson("["+raw+"]",&e); if(e.error!=QJsonParseError::NoError||!d.isArray())return false; const auto key=d.array().first().toString(); if(seen.contains(key))return false; seen.insert(key); ws(); if(p>=b.size()||b[p++]!=':')return false; if(!value(depth))return false; ws(); if(p>=b.size())return false; if(b[p]=='}'){++p;return true;} if(b[p++]!=',')return false; }
    }
    bool array(int depth) { if(depth>4||b[p++]!='[')return false; ws(); if(p<b.size()&&b[p]==']'){++p;return true;} while(true){if(!value(depth))return false;ws();if(p>=b.size())return false;if(b[p]==']'){++p;return true;}if(b[p++]!=',')return false;} }
};
}
