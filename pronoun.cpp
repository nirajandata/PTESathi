#include "env.h"
#include <chrono>
#include <condition_variable>
#include <crow.h>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <queue>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

using json = nlohmann::json;

const std::string subscriptionKey =
    SUBSCRIPTION_KEY;                // <-- Insert your subscription key
const std::string region = "eastus"; // <-- e.g., "eastus"
const std::string locale = "en-US";
const std::string audioFilePath = "meow.pcm";
const std::string referenceText = "morning morning.";
const size_t chunkSize = 1024;

std::vector<uint8_t> waveHeader16K16BitMono = {
    82, 73, 70, 70, 78, 128, 0,   0,  87,  65, 86, 69, 102, 109, 116, 32,
    18, 0,  0,  0,  1,  0,   1,   0,  128, 62, 0,  0,  0,   125, 0,   0,
    2,  0,  16, 0,  0,  0,   100, 97, 116, 97, 0,  0,  0,   0};

struct PronunciationAssessmentParams {
  std::string GradingSystem = "HundredMark";
  std::string Dimension = "Comprehensive";
  std::string ReferenceText = referenceText;
  std::string EnableProsodyAssessment = "true";
  std::string PhonemeAlphabet = "SAPI";
  std::string EnableMiscue = "true";
  std::string NBestPhonemeCount = "5";

  std::string toBase64Json() const {
    json j = {{"GradingSystem", GradingSystem},
              {"Dimension", Dimension},
              {"ReferenceText", ReferenceText},
              {"EnableProsodyAssessment", EnableProsodyAssessment},
              {"PhonemeAlphabet", PhonemeAlphabet},
              {"EnableMiscue", EnableMiscue},
              {"NBestPhonemeCount", NBestPhonemeCount}};

    std::string jsonStr = j.dump();
    std::string encoded;
    CURL *curl = curl_easy_init();
    if (curl) {
      char *output = curl_easy_escape(curl, jsonStr.c_str(), jsonStr.length());
      if (output) {
        encoded = output;
        curl_free(output);
      }
      curl_easy_cleanup(curl);
    }
    return encoded;
  }
};

std::string generateSessionID() {
  std::ostringstream oss;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint64_t> dis;
  oss << std::hex << dis(gen);
  return oss.str();
}

struct MemoryStream {
  std::ifstream file;
  std::vector<uint8_t> header;
  size_t chunkSize;

  MemoryStream(const std::string &path, const std::vector<uint8_t> &header,
               size_t chunkSize)
      : file(path, std::ios::binary), header(header), chunkSize(chunkSize) {}

  static size_t readCallback(void *ptr, size_t size, size_t nmemb,
                             void *userp) {
    MemoryStream *stream = static_cast<MemoryStream *>(userp);
    static bool headerSent = false;
    if (!headerSent) {
      memcpy(ptr, stream->header.data(), stream->header.size());
      headerSent = true;
      return stream->header.size();
    }

    if (!stream->file.is_open() || stream->file.eof()) {
      return 0;
    }

    stream->file.read(reinterpret_cast<char *>(ptr), size * nmemb);
    std::this_thread::sleep_for(std::chrono::milliseconds(
        stream->chunkSize * 1000 / 32000)); // simulate streaming
    return stream->file.gcount();
  }
};

int main() {
  curl_global_init(CURL_GLOBAL_ALL);

  // Open audio stream
  if (!std::filesystem::exists(audioFilePath)) {
    std::cerr << "File not found: " << audioFilePath << std::endl;
    return 1;
  }

  PronunciationAssessmentParams params;
  std::string paramsBase64 = params.toBase64Json();
  std::string sessionID = generateSessionID();

  std::string url = "https://" + region +
                    ".stt.speech.microsoft.com/speech/recognition/conversation/"
                    "cognitiveservices/v1?format=detailed&language=" +
                    locale + "&X-ConnectionId=" + sessionID;

  CURL *curl = curl_easy_init();
  if (!curl) {
    std::cerr << "Curl init failed" << std::endl;
    return 1;
  }

  MemoryStream stream(audioFilePath, waveHeader16K16BitMono, chunkSize);

  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Accept: application/json;text/xml");
  headers = curl_slist_append(headers, "Connection: Keep-Alive");
  headers = curl_slist_append(
      headers, "Content-Type: audio/wav; codecs=audio/pcm; samplerate=16000");
  headers = curl_slist_append(
      headers, ("Ocp-Apim-Subscription-Key: " + subscriptionKey).c_str());
  headers = curl_slist_append(
      headers, ("Pronunciation-Assessment: " + paramsBase64).c_str());
  headers = curl_slist_append(headers, "Transfer-Encoding: chunked");
  headers = curl_slist_append(headers, "Expect: 100-continue");

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, MemoryStream::readCallback);
  curl_easy_setopt(curl, CURLOPT_READDATA, &stream);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

  auto start = std::chrono::high_resolution_clock::now();

  std::string responseData;
  curl_easy_setopt(
      curl, CURLOPT_WRITEFUNCTION,
      +[](void *contents, size_t size, size_t nmemb, std::string *s) -> size_t {
        size_t totalSize = size * nmemb;
        s->append((char *)contents, totalSize);
        return totalSize;
      });
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);

  CURLcode res = curl_easy_perform(curl);
  auto end = std::chrono::high_resolution_clock::now();

  if (res != CURLE_OK) {
    std::cerr << "Request failed: " << curl_easy_strerror(res) << std::endl;
  } else {
    long httpCode;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    std::cout << "Session ID: " << sessionID << std::endl;
    if (httpCode != 200) {
      std::cerr << "HTTP " << httpCode << ": " << responseData << std::endl;
    } else {
      std::cout << "Response: " << responseData << std::endl;
      auto latency =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
              .count();
      std::cout << "Latency: " << latency << " ms" << std::endl;
    }
  }

  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);
  curl_global_cleanup();

  return 0;
}
