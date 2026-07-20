// Implements bounded JSON object parsing without a filesystem or JSON library.
#include "firmware/core/json_input.hpp"

#include <cctype>
#include <cstdlib>
#include <string>

namespace firmware::core {
namespace {

// Parses one JSON value while retaining scalar values for the current member.
class JsonParser {
public:
    explicit JsonParser(std::string_view text) : text_(text) {}

    // Parses any one JSON value, retaining members only for a top-level object.
    std::optional<JsonDocument> parse_document() {
        skip_whitespace();
        if (position_ >= text_.size()) {
            return std::nullopt;
        }
        if (text_[position_] == '{') {
            return parse_object();
        }
        JsonMember ignored{};
        if (!parse_value(ignored)) {
            return std::nullopt;
        }
        return JsonDocument{};
    }

    std::optional<JsonDocument> parse_object() {
        skip_whitespace();
        if (!consume('{')) {
            return std::nullopt;
        }
        JsonDocument document;
        skip_whitespace();
        if (consume('}')) {
            return document;
        }
        while (position_ < text_.size()) {
            const auto name = parse_string();
            if (!name.has_value()) {
                return std::nullopt;
            }
            skip_whitespace();
            if (!consume(':')) {
                return std::nullopt;
            }
            JsonMember member{*name, std::nullopt, std::nullopt};
            if (!parse_value(member)) {
                return std::nullopt;
            }
            document.members.push_back(std::move(member));
            skip_whitespace();
            if (consume('}')) {
                return document;
            }
            if (!consume(',')) {
                return std::nullopt;
            }
            skip_whitespace();
        }
        return std::nullopt;
    }

private:
    // Consumes one JSON value and stores only direct numeric or string scalars.
    bool parse_value(JsonMember& member) {
        skip_whitespace();
        if (position_ >= text_.size()) {
            return false;
        }
        if (text_[position_] == '"') {
            member.string = parse_string();
            return member.string.has_value();
        }
        if (text_[position_] == '{') {
            return skip_composite('{', '}');
        }
        if (text_[position_] == '[') {
            return skip_composite('[', ']');
        }
        if (text_.substr(position_, 4U) == "true") {
            position_ += 4U;
            return true;
        }
        if (text_.substr(position_, 5U) == "false") {
            position_ += 5U;
            return true;
        }
        if (text_.substr(position_, 4U) == "null") {
            position_ += 4U;
            return true;
        }
        return parse_number(member.number);
    }

    // Parses a JSON string with the standard short escapes used by request data.
    std::optional<std::string> parse_string() {
        if (!consume('"')) {
            return std::nullopt;
        }
        std::string value;
        while (position_ < text_.size()) {
            const char character = text_[position_++];
            if (character == '"') {
                return value;
            }
            if (character == '\\') {
                if (position_ >= text_.size()) {
                    return std::nullopt;
                }
                const char escaped = text_[position_++];
                switch (escaped) {
                    case '"': value.push_back('"'); break;
                    case '\\': value.push_back('\\'); break;
                    case '/': value.push_back('/'); break;
                    case 'b': value.push_back('\b'); break;
                    case 'f': value.push_back('\f'); break;
                    case 'n': value.push_back('\n'); break;
                    case 'r': value.push_back('\r'); break;
                    case 't': value.push_back('\t'); break;
                    default: return std::nullopt;
                }
            } else {
                if (static_cast<unsigned char>(character) < 0x20U) {
                    return std::nullopt;
                }
                value.push_back(character);
            }
        }
        return std::nullopt;
    }

    // Parses one JSON number using the C library's locale-independent syntax.
    bool parse_number(std::optional<double>& result) {
        const std::size_t start = position_;
        while (position_ < text_.size() &&
               (std::isdigit(static_cast<unsigned char>(text_[position_])) != 0 ||
                text_[position_] == '-' || text_[position_] == '+' ||
                text_[position_] == '.' || text_[position_] == 'e' ||
                text_[position_] == 'E')) {
            ++position_;
        }
        if (start == position_) {
            return false;
        }
        const std::string number(text_.substr(start, position_ - start));
        char* end = nullptr;
        const double value = std::strtod(number.c_str(), &end);
        if (end == number.c_str() || *end != '\0') {
            return false;
        }
        result = value;
        return true;
    }

    // Skips a nested object or array while validating balanced JSON structure.
    bool skip_composite(char opening, char closing) {
        if (!consume(opening)) {
            return false;
        }
        int depth = 1;
        bool in_string = false;
        bool escaped = false;
        while (position_ < text_.size() && depth > 0) {
            const char character = text_[position_++];
            if (in_string) {
                if (escaped) {
                    escaped = false;
                } else if (character == '\\') {
                    escaped = true;
                } else if (character == '"') {
                    in_string = false;
                }
            } else if (character == '"') {
                in_string = true;
            } else if (character == opening) {
                ++depth;
            } else if (character == closing) {
                --depth;
            }
        }
        return depth == 0 && !in_string && !escaped;
    }

    // Skips JSON whitespace before the next token.
    void skip_whitespace() {
        while (position_ < text_.size() &&
               std::isspace(static_cast<unsigned char>(text_[position_])) != 0) {
            ++position_;
        }
    }

    // Consumes an exact syntax character when present.
    bool consume(char expected) {
        if (position_ >= text_.size() || text_[position_] != expected) {
            return false;
        }
        ++position_;
        return true;
    }

    std::string_view text_;
    std::size_t position_ = 0U;
};

// Compares ASCII member names without changing case-sensitive string values.
bool names_equal(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < left.size(); ++index) {
        const auto left_character = static_cast<unsigned char>(left[index]);
        const auto right_character = static_cast<unsigned char>(right[index]);
        if (std::tolower(left_character) != std::tolower(right_character)) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::optional<JsonDocument> parse_json_prefix(BytesView input) {
    std::size_t length = 0U;
    while (length < input.size() && input[length] != 0U) {
        ++length;
    }
    return JsonParser(std::string_view(
               reinterpret_cast<const char*>(input.data()), length))
        .parse_document();
}

std::optional<double> find_json_number(const JsonDocument& document,
                                       std::string_view member_name) {
    for (const JsonMember& member : document.members) {
        if (names_equal(member.name, member_name)) {
            return member.number;
        }
    }
    return std::nullopt;
}

std::optional<std::string> find_json_string(const JsonDocument& document,
                                            std::string_view member_name) {
    for (const JsonMember& member : document.members) {
        if (names_equal(member.name, member_name)) {
            return member.string;
        }
    }
    return std::nullopt;
}

}  // namespace firmware::core
