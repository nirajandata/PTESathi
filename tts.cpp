#include "env.h"
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <string>

#include "env.h"

// Callback to write the audio data to a file
size_t writeData(void *ptr, size_t size, size_t nmemb, void *userdata) {
  std::ofstream *out = static_cast<std::ofstream *>(userdata);
  size_t totalSize = size * nmemb;
  out->write(static_cast<char *>(ptr), totalSize);
  return totalSize;
}

int main() {

  std::string content = "my voice is my passport verify me";

  std::string ssml = "<speak version='1.0' xml:lang='en-US'>"
                     "<voice xml:lang='en-US' xml:gender='Female' "
                     "name='en-US-AvaMultilingualNeural'>" +
                     content +
                     "</voice>"
                     "</speak>";

  CURL *curl = curl_easy_init();
  if (!curl) {
    std::cerr << "Failed to initialize curl\n";
    return 1;
  }

  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(
      headers, ("Ocp-Apim-Subscription-Key: " + SUBSCRIPTION_KEY).data());

  headers = curl_slist_append(headers, "Content-Type: application/ssml+xml");
  headers = curl_slist_append(
      headers, "X-Microsoft-OutputFormat: audio-16khz-128kbitrate-mono-mp3");
  headers = curl_slist_append(headers, "User-Agent: curl");

  std::ofstream outfile("output.mp3", std::ios::binary);
  if (!outfile) {
    std::cerr << "Failed to open output file\n";
    return 1;
  }

  curl_easy_setopt(curl, CURLOPT_URL, URL.data());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ssml.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, ssml.size());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outfile);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res)
              << "\n";
  } else {
    std::cout << "Audio saved to output.mp3\n";
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  outfile.close();
  return 0;
}
