#include "zm.h"
# include "zm_crypt.h"
#include "BCrypt.hpp"
#include "jwt.h"
#include <algorithm>
#include <openssl/sha.h>


// returns username if valid, "" if not
std::string verifyToken(std::string jwt_token_str, std::string key) {
  std::string username = "";
  try {
    // is it decodable?
    auto decoded = jwt::decode(jwt_token_str);
    auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs256{ key })
                        .with_issuer("ZoneMinder");
  
    // signature verified?
    verifier.verify(decoded);

    // make sure it has fields we need
    if (decoded.has_payload_claim("type")) {
      std::string type = decoded.get_payload_claim("type").as_string();
      if (type != "access") {
        Error ("Only access tokens are allowed. Please do not use refresh tokens");
        return "";
      }
    }
    else {
      // something is wrong. All ZM tokens have type
      Error ("Missing token type. This should not happen");
      return "";
    }
    if (decoded.has_payload_claim("user")) {
      username  = decoded.get_payload_claim("user").as_string();
      Info ("Got %s as user claim from token", username.c_str());
    } 
    else {
      Error ("User not found in claim");
      return "";
    }
  } // try
  catch (const std::exception &e) {
      Error("Unable to verify token: %s", e.what());
      return "";
  }
  catch (...) {
     Error ("unknown exception");
     return "";

  }
  return username;
}

bool verifyPassword(const char *username, const char *input_password, const char *db_password_hash) {
  bool password_correct = false;
  if (strlen(db_password_hash ) < 4) {
    // actually, shoud be more, but this is min. for next code
    Error ("DB Password is too short or invalid to check");
    return false;
  }
  if (db_password_hash[0] == '*') {
    // MYSQL PASSWORD
    Info ("%s is using an MD5 encoded password", username);
    
    SHA_CTX ctx1, ctx2;
    unsigned char digest_interim[SHA_DIGEST_LENGTH];
    unsigned char digest_final[SHA_DIGEST_LENGTH];

    //get first iteration
    SHA1_Init(&ctx1);
    SHA1_Update(&ctx1, input_password, strlen(input_password));
    SHA1_Final(digest_interim, &ctx1);

    //2nd iteration
    SHA1_Init(&ctx2);
    SHA1_Update(&ctx2, digest_interim,SHA_DIGEST_LENGTH);
    SHA1_Final (digest_final, &ctx2);

    char final_hash[SHA_DIGEST_LENGTH * 2 +2];
    final_hash[0]='*';
    //convert to hex
    for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
         sprintf(&final_hash[i*2]+1, "%02X", (unsigned int)digest_final[i]);
    final_hash[SHA_DIGEST_LENGTH *2 + 1]=0;

    Info ("Computed password_hash:%s, stored password_hash:%s", final_hash,  db_password_hash);
    Debug (5, "Computed password_hash:%s, stored password_hash:%s", final_hash,  db_password_hash);
    password_correct = (strcmp(db_password_hash, final_hash)==0);
  }
  else if ((db_password_hash[0] == '$') && (db_password_hash[1]== '2')
           &&(db_password_hash[3] == '$')) {
    // BCRYPT 
    Info ("%s is using a bcrypt encoded password", username);
    BCrypt bcrypt;
    std::string input_hash = bcrypt.generateHash(std::string(input_password));
    password_correct = bcrypt.validatePassword(std::string(input_password), std::string(db_password_hash));
  }
  else {
    // plain
    Warning ("%s is using a plain text password, please do not use plain text", username);
    password_correct = (strcmp(input_password, db_password_hash) == 0);
  }
  return password_correct;
}