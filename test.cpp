#include "env.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <curl/curl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp> // Include nlohmann/json.hpp for JSON handling
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <sstream>
#include <string>
#include <vector>

using json = nlohmann::json;

// Function to base64-encode a string
std::string base64_encode(const std::string &input) {
  BIO *bio, *b64;
  BUF_MEM *bufferPtr;
  b64 = BIO_new(BIO_f_base64());
  bio = BIO_new(BIO_s_mem());
  bio = BIO_push(b64, bio);
  BIO_set_flags(bio,
                BIO_FLAGS_BASE64_NO_NL); // Do not use newlines to flush buffer
  BIO_write(bio, input.c_str(), static_cast<int>(input.length()));
  BIO_flush(bio);
  BIO_get_mem_ptr(bio, &bufferPtr);
  std::string encoded_data(bufferPtr->data, bufferPtr->length);
  BIO_free_all(bio);
  return encoded_data;
}

// WAV header for 16kHz, 16-bit mono PCM
const uint8_t waveHeader16K16BitMono[] = {
    82, 73, 70, 70, 78, 128, 0,   0,  87,  65, 86, 69, 102, 109, 116, 32,
    18, 0,  0,  0,  1,  0,   1,   0,  128, 62, 0,  0,  0,   125, 0,   0,
    2,  0,  16, 0,  0,  0,   100, 97, 116, 97, 0,  0,  0,   0};

// Structure to hold audio data and reading state
struct AudioData {
  std::ifstream file;
  std::vector<uint8_t> header;
  bool header_sent = false;
  size_t chunk_size = 1024;
};

// Read callback function for libcurl
size_t read_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  AudioData *audio = static_cast<AudioData *>(userdata);
  size_t max_size = size * nmemb;

  if (!audio->header_sent) {
    size_t header_size = audio->header.size();
    if (header_size > max_size) {
      std::cerr << "Header size exceeds buffer size." << std::endl;
      return CURL_READFUNC_ABORT;
    }
    std::memcpy(ptr, audio->header.data(), header_size);
    audio->header_sent = true;
    return header_size;
  }

  if (audio->file.eof()) {
    return 0;
  }

  audio->file.read(ptr, std::min(audio->chunk_size, max_size));
  return static_cast<size_t>(audio->file.gcount());
}

int main() {
  const std::string subscriptionKey = SUBSCRIPTION_KEY;
  const std::string region = "eastus"; // Replace with your region
  const std::string locale = "en-US";
  const std::string audioFilePath = "meow.pcm";
  const std::string referenceText = "meow meow";

  // Open audio file
  std::ifstream audioFile(audioFilePath, std::ios::binary);
  if (!audioFile) {
    std::cerr << "Failed to open audio file." << std::endl;
    return 1;
  }

  // Prepare pronunciation assessment parameters
  json params = {
      {"GradingSystem", "HundredMark"}, {"Dimension", "Comprehensive"},
      {"ReferenceText", referenceText}, {"EnableProsodyAssessment", "true"},
      {"PhonemeAlphabet", "SAPI"},      {"EnableMiscue", "true"},
      {"NBestPhonemeCount", "5"}};
  std::string params_json = params.dump();
  std::string params_base64 = base64_encode(params_json);

  // Generate session ID
  std::srand(static_cast<unsigned int>(std::time(nullptr)));
  std::stringstream ss;
  ss << std::hex << std::rand();
  std::string sessionID = ss.str();

  // Build request URL
  std::string url = "https://" + region +
                    ".stt.speech.microsoft.com/speech/recognition/conversation/"
                    "cognitiveservices/v1"
                    "?format=detailed&language=" +
                    locale + "&X-ConnectionId=" + sessionID;

  // Initialize libcurl
  CURL *curl = curl_easy_init();
  if (!curl) {
    std::cerr << "Failed to initialize libcurl." << std::endl;
    return 1;
  }

  // Set headers
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Accept: application/json;text/xml");
  headers = curl_slist_append(headers, "Connection: Keep-Alive");
  headers = curl_slist_append(
      headers, "Content-Type: audio/wav; codecs=audio/pcm; samplerate=16000");
  headers = curl_slist_append(
      headers, ("Ocp-Apim-Subscription-Key: " + subscriptionKey).c_str());
  headers = curl_slist_append(
      headers, ("Pronunciation-Assessment: " + params_base64).c_str());
  headers = curl_slist_append(headers, "Transfer-Encoding: chunked");
  headers = curl_slist_append(headers, "Expect: 100-continue");

  // Prepare audio data
  AudioData audioData;
  audioData.file = std::move(audioFile);
  audioData.header.assign(std::begin(waveHeader16K16BitMono),
                          std::end(waveHeader16K16BitMono));

  // Set libcurl options
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
  curl_easy_setopt(curl, CURLOPT_READDATA, &audioData);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

  // Perform the request
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::cerr << "libcurl error: " << curl_easy_strerror(res) << std::endl;
  }

  // Cleanup
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  return 0;
}
