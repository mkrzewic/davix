/*
 * This File is part of Davix, The IO library for HTTP based protocols
 * Copyright (C) CERN 2018
 * Author: Georgios Bitzes <georgios.bitzes@cern.ch>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
*/

#include <utils/davix_gcloud_utils.hpp>
#include <libs/alibxx/crypto/hmacsha.hpp>
#include <libs/alibxx/crypto/base64.hpp>
#include <fstream>
#include <libs/rapidjson/document.h>
#include <sstream>

#define SSTR(message) static_cast<std::ostringstream&>(std::ostringstream().flush() << message).str()


namespace Davix{

namespace gcloud {

class CredentialsInternal {
public:
  CredentialsInternal() {}
  std::string private_key;
  std::string client_email;
};

bool Credentials::isEmpty() const {
  return internal->private_key.empty() && internal->client_email.empty();
}

Credentials::Credentials() {
  internal = new CredentialsInternal();
}

// Destructor
Credentials::~Credentials() {
  delete internal;
  internal = NULL;
}

// Copy constructor
Credentials::Credentials(const Credentials& other) {
  internal = new CredentialsInternal(*other.internal);
}

// Move constructor
Credentials::Credentials(Credentials&& other) {
  internal = other.internal;
  other.internal = new CredentialsInternal();

}

// Copy assignment operator
Credentials& Credentials::operator=(const Credentials& other) {
  internal = new CredentialsInternal(*other.internal);
  return *this;
}

// Move assignment operator
Credentials& Credentials::operator=(Credentials&& other) {
  internal = other.internal;
  other.internal = new CredentialsInternal();
  return *this;
}

void Credentials::setPrivateKey(const std::string &str) {
  internal->private_key = str;
}

std::string Credentials::getPrivateKey() const {
  return internal->private_key;
}

void Credentials::setClientEmail(const std::string &str) {
  internal->client_email = str;
}

std::string Credentials::getClientEmail() const {
  return internal->client_email;
}

CredentialProvider::CredentialProvider() {}

Credentials CredentialProvider::fromJSONString(const std::string &str) {
  Credentials creds;

  rapidjson::Document document;
  if(document.Parse(str.c_str()).HasParseError()) {
    throw DavixException(std::string("davix::gcloud"), StatusCode::ParsingError, "Error during JSON parsing");
  }

  if(!document.HasMember("private_key")) {
    throw DavixException(std::string("davix::gcloud"), StatusCode::ParsingError, "Error during JSON parsing: Could not find private_key");
  }

  if(!document.HasMember("client_email")) {
    throw DavixException(std::string("davix::gcloud"), StatusCode::ParsingError, "Error during JSON parsing: Could not find client_email");
  }

  creds.setPrivateKey(document["private_key"].GetString());
  creds.setClientEmail(document["client_email"].GetString());

  return creds;
}

Credentials CredentialProvider::fromFile(const std::string &path) {
  std::stringstream buffer;

  try {
    std::ifstream t(path);
    buffer << t.rdbuf();
  }
  catch(...) {
    throw DavixException(std::string("davix::gcloud"), StatusCode::FileNotFound, SSTR("Could not read gcloud credentials at '" << path << "'"));
  }

  return fromJSONString(buffer.str());
}

std::string getStringToSign(const std::string &verb, const Uri &url, const HeaderVec &headers, const time_t expirationTime) {
  std::ostringstream ss;

  // Reference: https://cloud.google.com/storage/docs/access-control/create-signed-urls-program
  // a. Add HTTP verb
  ss << verb << "\n";

  // b. MD5 digest value - empty
  ss << "\n";

  // c. Content-Type - empty
  ss << "\n";

  // d. Expiration
  ss << expirationTime << "\n";

  // Resource
  ss << url.getPath();

  return ss.str();
}

Uri signURI(const Credentials& creds, const std::string &verb, const Uri &url, const HeaderVec &headers, const time_t expirationTime) {
  // Reference: https://cloud.google.com/storage/docs/access-control/create-signed-urls-program
  // Construct string to sign..
  std::string stringToSign = getStringToSign(verb, url, headers, expirationTime);

  // Calculate signature..
  std::string binarySignature = rsasha256(creds.getPrivateKey(), stringToSign);

  // Base64 encode signature..
  std::string signature = Base64::base64_encode( (unsigned char*) binarySignature.c_str(), binarySignature.size());

  Uri signedUrl(url);
  signedUrl.addQueryParam("GoogleAccessId", creds.getClientEmail());
  signedUrl.addQueryParam("Expires", SSTR(expirationTime));
  signedUrl.addQueryParam("Signature", signature);

  return signedUrl;
}

} // namespace gcloud
} // namespace Davix