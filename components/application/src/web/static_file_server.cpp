/** @file @brief Implements static-file lookup, MIME selection, chunking, and close handling. */
#include "application/web/static_file_server.hpp"

#include "core/web/web_static.hpp"

namespace firmware::application {

void StaticFileServer::serve(std::string_view request_uri, StaticFilePort& file,
                             StaticFileResponsePort& response) const {
    const auto path = core::resolve_static_path(request_uri);
    if (!path.has_value()) {
        response.send_error(404U, "text/html", core::web::missing_static_file_body);
        return;
    }
    if (!file.open(*path).has_value()) {
        response.send_error(404U, "text/html", core::web::missing_static_file_body);
        return;
    }

    response.begin_chunked(core::static_mime_type(*path));
    while (true) {
        const auto chunk = file.read(core::web::static_file_chunk_size);
        if (!chunk.has_value()) {
            break;
        }
        response.send_chunk(*chunk);
        if (chunk->empty()) {
            break;
        }
    }
    file.close();
    response.finish_chunks();
}

}  // namespace firmware::application
