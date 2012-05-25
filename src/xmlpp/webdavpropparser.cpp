#define _GNU_SOURCE

#include "webdavpropparser.h"
#include "davixxmlparserexception.h"



#include <glibmm/quark.h>
#include <glibmm/datetime.h>
#include <glibmm.h>
#include <cstring>


#include <datetime/datetime_utils.h>

namespace Davix {

// static string initialization to skip heavy stupid copy
const Glib::ustring prop_pattern("prop");
const Glib::ustring propstat_pattern("propstat");
const Glib::ustring response_pattern("response");
const Glib::ustring getlastmodified_pattern("getlastmodified");
const Glib::ustring creationdate_pattern("creationdate");
const Glib::ustring getcontentlength_pattern("getcontentlength");
const Glib::ustring mode_pattern("mode");
const Glib::ustring href_pattern("href");


inline bool match_element(const Glib::ustring & origin, const Glib::ustring& pattern){ // C style optimized, critical function
    bool res = false;
    const char* c_origin = (char*) origin.c_str();
    const char* c_pattern = (char*) pattern.c_str();
    const char* pos = strrchr(c_origin, ':');
    if(pos != NULL){
        res = (strcmp(pos+1, c_pattern) ==0)?true:false;
    }
    return res;
}

/**
  check if the current element origin match the pattern
  if it is the case, force the scope_bool -> TRUE, if already to TRUE -> error
  in a case of a change, return true, else false
*/
inline bool add_scope(bool *scope_bool, const Glib::ustring& origin, const Glib::ustring&pattern){
    if(match_element(origin, pattern)){
        if(*scope_bool==true){
            throw DavixXmlParserException(Glib::Quark("WebdavPropParser::add_scope"), EINVAL,
                       Glib::ustring::compose(" parsing error in the webdav request result, <%1> duplicated ", origin));
        }
        //std::cout << " in scope " << origin << " while parsing " << std::endl;
        *scope_bool = true;
        return true;
    }
    return false;
}

/**
  act like add_scope but in the opposite way
*/
inline bool remove_scope(bool *scope_bool, const Glib::ustring& origin, const Glib::ustring& pattern){
    if(match_element(origin, pattern)){
        if(*scope_bool==false){
           throw DavixXmlParserException(Glib::Quark("WebdavPropParser::remove_scope"), EINVAL,
                       Glib::ustring::compose(" parsing error in the webdav request result, </%1> not open before ", origin));
        }
        //std::cout << " out scope " << origin << " while parsing " << std::endl;
        *scope_bool = false;
        return true;
    }
    return false;
}



WebdavPropParser::WebdavPropParser()
{
}
WebdavPropParser::~WebdavPropParser()
{
}

void WebdavPropParser::on_start_document(){
    davix_log_debug(" -> parse request for properties ");
    prop_section = propname_section = false;
    response_section = lastmod_section = false;
    creatdate_section = contentlength_section= false;
    mode_ext_section = href_section = false;
}

void WebdavPropParser::on_end_document(){
    davix_log_debug(" -> end of parse request for properties ");
}

void WebdavPropParser::on_start_element(const Glib::ustring& name, const AttributeList& attributes){
    //std::cout << " name : " << name << std::endl;
    // compute the current scope
    bool new_prop= add_scope(&prop_section, name, prop_pattern);
    add_scope(&propname_section, name, propstat_pattern);
    add_scope(&response_section, name, response_pattern);
    add_scope(&lastmod_section, name, getlastmodified_pattern);
    add_scope(&creatdate_section, name, creationdate_pattern);
    add_scope(&contentlength_section, name, getcontentlength_pattern);
    add_scope(&mode_ext_section, name, mode_pattern); // lcgdm extension for mode_t support
    add_scope(&href_section, name, href_pattern);
    // collect information
    if(new_prop)
        compute_new_elem();
}

void WebdavPropParser::on_end_element(const Glib::ustring& name){
    // std::cout << "name : " << name << std::endl;
    // compute the current scope
    bool end_prop = remove_scope(&prop_section, name, prop_pattern);
    remove_scope(&propname_section, name, propstat_pattern);
    remove_scope(&response_section, name, response_pattern);
    remove_scope(&lastmod_section, name, getlastmodified_pattern);
    remove_scope(&creatdate_section, name, creationdate_pattern);
    remove_scope(&contentlength_section, name, getcontentlength_pattern);
    remove_scope(&mode_ext_section, name, mode_pattern); // lcgdm extension for mode_t support
    remove_scope(&href_section, name, href_pattern);

    if(end_prop)
        store_new_elem();

}

const std::vector<FileProperties> & WebdavPropParser::parser_properties_from_memory(const Glib::ustring& str){
    _props.clear();
    parse_memory(str);
    return _props;
}

const std::vector<FileProperties> & WebdavPropParser::parser_properties_from_chunk(const Glib::ustring& str){
    parse_chunk(str);
    return _props;
}

void WebdavPropParser::clean(){
    _props.clear();
}

const std::vector<FileProperties> & WebdavPropParser::get_current_properties(){
    return _props;
}

void WebdavPropParser::compute_new_elem(){
    if(prop_section && propname_section && response_section){
        davix_log_debug(" properties detected ");
        _current_props.clear();
        _current_props.filename = last_filename; // setup the current filename
    }
}

void WebdavPropParser::store_new_elem(){
    if(propname_section && response_section){
        davix_log_debug(" end of properties... ");
        _props.push_back(_current_props);
    }
}

void WebdavPropParser::check_last_modified(const Glib::ustring& chars){
    if(response_section && prop_section && propname_section
          && lastmod_section){ // parse rfc1123 date format
        davix_log_debug(" getlastmodified found -> parse it ");
        GError * tmp_err=NULL;
        time_t t = parse_http_date(chars.c_str(), &tmp_err);
        if(t == -1){
            DavixXmlParserException ex(g_quark_to_string(tmp_err->domain), ECOMM, tmp_err->message);
            g_clear_error(&tmp_err);
            throw (ex);
        }
        davix_log_debug(" getlastmodified found -> value %ld ", t);
        _current_props.mtime = t;
    }
}

void WebdavPropParser::check_creation_date(const Glib::ustring& chars){
    if(response_section && prop_section && propname_section
            && creatdate_section){
        davix_log_debug("creationdate found -> parse it");
        GError * tmp_err=NULL;
        time_t t = parse_iso8601date(chars.c_str(), &tmp_err);
        if(t == -1){
            DavixXmlParserException ex(g_quark_to_string(tmp_err->domain), ECOMM, tmp_err->message);
            g_clear_error(&tmp_err);
            throw ex;
        }
        davix_log_debug(" creationdate found -> value %ld ", t);
        _current_props.ctime = t;
    }
}

void WebdavPropParser::check_content_length(const Glib::ustring& chars){
    if(response_section && prop_section && propname_section
             && contentlength_section){
        davix_log_debug(" content length found -> parse it");
        const unsigned long mysize = strtoul(chars.c_str(), NULL, 10);
        if(mysize == ULONG_MAX){
            errno =0;
            throw DavixXmlParserException(Glib::Quark("WebdavPropParser::check_content_length"), ECOMM, " Invalid content length value in dav response");
        }
        davix_log_debug(" content length found -> %ld", mysize);
        _current_props.size = (off_t) mysize;
    }
}

void WebdavPropParser::check_mode_ext(const Glib::ustring& chars){
    if(response_section && prop_section && propname_section &&
            mode_ext_section){
        davix_log_debug(" mode_t extension for LCGDM found -> parse it");
        const mode_t mymode = strtoul(chars.c_str(), NULL, 8);
        if(mymode == ULONG_MAX){
              throw DavixXmlParserException(Glib::Quark("WebdavPropParser::check_mode_ext"), ECOMM, " Invalid mode_t value for the LCGDM extension");
        }
        davix_log_debug(" mode_t extension found -> 0%o", mymode);
        _current_props.mode = mymode;
    }
}

void WebdavPropParser::check_href(const Glib::ustring &name){
    if(response_section &&
            href_section){
        davix_log_debug(" href/filename found -> parse it");
       last_filename = name;
       size_t s = last_filename.size();
       while(s != 0 && last_filename[s-1] == '/' ){
            last_filename.erase(s-1);
            s= last_filename.size();
       }
       if( (s = last_filename.rfind('/')) != Glib::ustring::npos)
           last_filename = last_filename.substr(s+1);


       davix_log_debug(" href/filename found -> %s ", last_filename.c_str() );
    }
}

void WebdavPropParser::on_characters(const Glib::ustring& characters){
    //std::cout << "chars ..." << characters<< std::endl;
    check_last_modified(characters);
    check_creation_date(characters);
    check_content_length(characters);
    check_mode_ext(characters);
    check_href(characters);
}



} // namespace DAvix
