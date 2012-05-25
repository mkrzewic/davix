#ifndef DAVIX_NEONREQUEST_H
#define DAVIX_NEONREQUEST_H

#include <vector>
#include <utility>
#include <queue>
#include <neon/ne_request.h>
#include <neon/neonsessionfactory.h>

#include <global_def.h>
#include <httprequest.h>

namespace Davix {

#define NEON_BUFFER_SIZE 65000

class NEONSessionFactory;

class NEONRequest : public HttpRequest
{
public:
    NEONRequest(NEONSessionFactory* f, ne_session * sess, RequestType typ, const std::string & path,
                void * user_auth_callback_data,
                davix_auth_callback call);
    virtual ~NEONRequest();

    /**
      add a personalized header to the header request
      replace an existing one if already exist
      remove one if empty
      @param field : add a header field to the current http request or replace a existing one
      @param value : value of the field to set

    */
    virtual void add_header_field(const std::string & field, const std::string & value);
    /**
     * set the request command to execute ( GET, POST, PUT, PROPFIND )
     */
    virtual void set_requestcustom(const std::string & request_str);

    /**
      disable the certificate authority validity check for the https request
    */
    virtual void disable_ssl_ca_check();

    /**
      Execute the given request and return result to the buffer result
      Execute the constructed query, throw an exception if an error occures
      @return 0 on success
      @throw Glib::Error : error string and protocol error code
     */
    virtual int execute_sync() ; // throw(Glib::Error)

    virtual void add_full_request_content(const std::string & body);

    virtual void execute_block();
    /**
      read a block of a maximum size bytes in the request
      @param buffer : buffer to fill
      @param max_size : maximum number of bytes to set
      @throw Glib::Error
    */
    virtual ssize_t read_block(char* buffer, size_t max_size);
    /**
      finish an already started request
     */
    virtual void finish_block();
    /**
      get a reference to the current result for synchronous full request
     */
    virtual const std::vector<char> & get_result();
    /**
      clear the current result
    */
    virtual void clear_result();



    /**
      return the current request code error
       ex : HTTP 200
     */
    virtual int get_request_code();

    /**
      reimplement authentification
    */
    virtual void try_set_pkcs12_cert(const char * filename_pkcs12, const char* passwd);
protected:
    ne_session *    _sess;
    ne_request * _req;
    std::string  _path;
    std::vector<char> _vec;
    std::string _content_body;
    std::string _request_type;
    NEONSessionFactory* _f;

    std::vector< std::pair<std::string, std::string > > _headers_field;

    void * _user_auth_callback_data;
    davix_auth_callback _call;


    void create_req();
    /**
      internal, try to authentification with pkcs12 credential
    */
    int try_pkcs12_authentification(ne_session *sess, const ne_ssl_dname *const *dnames);
    /**
      main libneon callback for clicert
     */
    static void provide_clicert_fn(void *userdata, ne_session *sess,
                              const ne_ssl_dname *const *dnames,
                              int dncount);

};

/**
  translate a neon function status to a string and a errno code
 */
std::string  translate_neon_status(int ne_status, ne_session* sess, int* errno_code);


} // namespace Davix

#endif // DAVIX_NEONREQUEST_H