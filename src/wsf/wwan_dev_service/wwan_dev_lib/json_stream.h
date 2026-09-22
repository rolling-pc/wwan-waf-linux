#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QList>
#include <cctype>

// Split concatenated / partial local-socket JSON objects.
// Brace-matched slices that are not valid JSON (for example a GUID "{88514C6A-...}"
// left over after a torn read) are not consumed as messages: only the leading '{'
// is skipped so a following real object can still be recovered.
inline QList<QByteArray> extractJsonObjects(QByteArray& buffer) {
    QList<QByteArray> objects;
    int consumed = 0;
    int i = 0;
    const int size = buffer.size();
    while (i < size) {
        while (i < size && isspace(static_cast<unsigned char>(buffer.at(i)))) {
            ++i;
        }
        if (i >= size) {
            consumed = i;
            break;
        }
        if (buffer.at(i) != '{') {
            ++i;
            consumed = i;
            continue;
        }
        int depth = 0;
        bool inString = false;
        bool escape = false;
        const int start = i;
        bool complete = false;
        for (; i < size; ++i) {
            const char c = buffer.at(i);
            if (inString) {
                if (escape) {
                    escape = false;
                } else if (c == '\\') {
                    escape = true;
                } else if (c == '"') {
                    inString = false;
                }
                continue;
            }
            if (c == '"') {
                inString = true;
            } else if (c == '{') {
                ++depth;
            } else if (c == '}') {
                --depth;
                if (depth == 0) {
                    ++i;
                    complete = true;
                    break;
                }
            }
        }
        if (!complete) {
            break;
        }
        const QByteArray slice = buffer.mid(start, i - start);
        const QJsonDocument doc = QJsonDocument::fromJson(slice);
        if (doc.isObject() || doc.isArray()) {
            objects.append(slice);
            consumed = i;
        } else {
            i = start + 1;
            consumed = i;
        }
    }
    if (consumed > 0) {
        buffer.remove(0, consumed);
    }
    return objects;
}
