package io.livekit.server;

import java.util.Map;
import java.util.stream.Collectors;

/** Small encoder for binding options; protocol payloads are passed through verbatim. */
final class Json {
    private Json() {}
    static String encode(Object value) {
        if (value == null) return "null";
        if (value instanceof Boolean || value instanceof Number) return value.toString();
        if (value instanceof Map<?, ?> map) return map.entrySet().stream()
            .map(e -> encode(e.getKey().toString()) + ":" + encode(e.getValue()))
            .collect(Collectors.joining(",", "{", "}"));
        if (value instanceof Iterable<?> list) {
            var out = new java.util.StringJoiner(",", "[", "]");
            list.forEach(v -> out.add(encode(v)));
            return out.toString();
        }
        String text = (String) value;
        var out = new StringBuilder("\"");
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            if (c == '"' || c == '\\') out.append('\\').append(c);
            else if (c < 32 || Character.isSurrogate(c)) out.append(String.format("\\u%04x", (int) c));
            else out.append(c);
        }
        return out.append('"').toString();
    }
}
