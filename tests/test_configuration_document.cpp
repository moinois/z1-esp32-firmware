// Verifies target-neutral config parsing, namespace isolation, and serialization.
#include "test.hpp"

#include "firmware/application/configuration_document.hpp"

using firmware::application::ConfigurationDocument;
using firmware::application::ConfigurationNamespace;

TEST_CASE(configuration_document_parses_and_preserves_unknown_lines) {
    const auto document = ConfigurationDocument::parse(
        "; comment\r\n camera_width = 640 \r\nunknown text\n");
    REQUIRE_EQ(*document.get("camera_width"), std::string("640"));
    REQUIRE_EQ(document.lines().size(), 3U);
    REQUIRE_EQ(document.serialize(),
               std::string("; comment\n camera_width = 640 \nunknown text\n"));
}

TEST_CASE(configuration_namespace_normalizes_tag_and_isolates_prefixes) {
    auto document = ConfigurationDocument::parse(
        "cam_width=1\ncamera_width=2\ncamera_height=3\n");
    ConfigurationNamespace camera(document, "camera");
    REQUIRE_EQ(*camera.get("width"), std::string("2"));
    REQUIRE_EQ(camera.get_all().size(), 2U);
    camera.set("width", "4");
    REQUIRE_EQ(*document.get("camera_width"), std::string("4"));
    REQUIRE_EQ(*document.get("cam_width"), std::string("1"));
}

TEST_CASE(configuration_document_returns_first_duplicate_and_appends_missing) {
    auto document = ConfigurationDocument::parse("key=first\nkey=second\n");
    REQUIRE_EQ(*document.get("key"), std::string("first"));
    document.set("new", "value");
    REQUIRE_EQ(*document.get("new"), std::string("value"));
}
