#include <was/blob.h>
#include <was/storage_account.h>
#include <iostream>
#include <fstream>
#include <curl/curl.h>
#include <vector>
#include "HTTPRequest.hpp"

using std::string;
using std::vector;

int main(int argc, char **argv) {
//    int M = atoi(argv[1]), R = atoi(argv[2]);
//    string containerName = argv[3];
//    string storage_connection_string = "DefaultEndpointsProtocol=https;AccountName=connoryin;AccountKey=PAkfxaOyc7Q+HAM0CJW20SFE6N4FviWF6wIaU5KpyJBw8jcTyCCY2U+Z80VRZUtrTGTibPXOjpob+AStkRj4PQ==;EndpointSuffix=core.windows.net";
//    azure::storage::cloud_storage_account storage_account = azure::storage::cloud_storage_account::parse(
//            storage_connection_string);
//    auto blob_client = storage_account.create_cloud_blob_client();
//    azure::storage::cloud_blob_container container = blob_client.get_container_reference(
//            _XPLATSTR(containerName));
//    container.create_if_not_exists();
//
//    vector<string> filenames(argc - 5);
//    for (int i = 5; i < argc; ++i) {
//        filenames[i - 5] = argv[i];
//    }
//
//    std::vector<long> sizes;
//    long size = 0;
//    std::vector<std::ifstream> ifs(filenames.size());
//    for (int i = 0; i < filenames.size(); ++i) {
//        ifs[i].open(filenames[i], std::fstream::ate);
//        sizes.push_back(ifs[i].tellg());
//        size += sizes.back();
//    }
//
//    long minSize = ceil(size / M);
//
//    string buffer;
//    long startPos = 0, endPos = 0;
//    int curFile = 0;
//
//    int fileIndex = 0;
//    for (int i = 0; i < M; ++i) {
//        long sizeRemaining = minSize;
//        while (sizeRemaining > 0) {
//            if (endPos < sizes[curFile]) {
//                startPos = endPos;
//                endPos = std::min(sizes[curFile], endPos + sizeRemaining);
//                ifs[curFile].seekg(endPos);
//                while (!ifs[curFile].eof() && ifs[curFile].get() != '\n') endPos++;
//                sizeRemaining -= (endPos - startPos);
//
//                ifs[curFile].clear();
//                ifs[curFile].seekg(startPos);
//                auto prevSize = buffer.size();
//                buffer.resize(buffer.size() + endPos - startPos);
//                ifs[curFile].read(buffer.data() + prevSize, endPos - startPos);
//            } else {
//                curFile++;
//                startPos = 0, endPos = 0;
//                if (curFile >= filenames.size()) break;
//            }
//        }
//        azure::storage::cloud_block_blob blob2 = container.get_block_blob_reference(
//                _XPLATSTR("input/" + std::to_string(fileIndex)));
//        blob2.upload_text(_XPLATSTR(buffer));
//        ++fileIndex;
//        buffer.clear();
//    }
//    for (auto &fs: ifs) fs.close();

    string url = "http://";
    url.append(argv[4]);
    url.append(":18080/");
    url.append(argv[1]);
    url.append("/");
    url.append(argv[2]);
    url.append("/");
    url.append(argv[3]);

    again:
    try {
        std::cout << "sending request to " << url << std::endl;
        http::Request request{url};
        const auto response = request.send("POST");

        if (response.headerFields.empty()) {
            std::cout << "master failed, retrying...\n";
            sleep(10);
            goto again;
        }
        std::cout << std::string{response.body.begin(), response.body.end()} << '\n';
    }
    catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
        std::cout << "master failed, retrying...\n";
        sleep(10);
        goto again;
    }
}