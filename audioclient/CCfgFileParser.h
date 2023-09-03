//
// Created by 997289110 on 12/23/19.
//

#ifndef AUDIOCLIENT_CCFGFILEPARSER_H
#define AUDIOCLIENT_CCFGFILEPARSER_H


#include   <stdlib.h>
#include   <stdio.h>

#ifdef   WIN32
#pragma   warning(disable:4786)
#pragma   warning(disable:4503)
#endif

#include   <map>
#include   <string>

class   CCfgFileParser
{
public:
    CCfgFileParser(bool   base64 = false,
                   char   chSectionBMark = '[',
                   char   chSectionEMark = ']',
                   char   chRecordEMark = ';',   //record   end-mark
                   char   chCommentMark = '#'
    )
            :m_base64(base64), m_chSectionBMark(chSectionBMark), m_chSectionEMark(chSectionEMark),
             m_chRecordEMark(chRecordEMark), m_chCommentMark(chCommentMark)
    {
    }

    virtual   ~CCfgFileParser()   {}

    typedef   std::map<std::string, std::string>   SECTION_CONTENT;
    typedef   std::map<std::string, SECTION_CONTENT   >   FILE_CONTENT;

private:
    bool m_base64;
    enum{
        SYNTAX_ERROR = 0x100,
    };

public:
    const   char*   getErrorString(int   err);
    int   parseFile(const   char*   lpszFilename);
    int   sections()   const   { return   m_content.size(); }

    FILE_CONTENT::const_iterator   sectionBegin()   const   { return   m_content.begin(); }
    FILE_CONTENT::const_iterator   sectionEnd()   const   { return   m_content.end(); }

    //return   empty-string   if   no   corresponding   value   presented.
    const char*  getValue(const   std::string&   strSection, const   std::string&   strKey)   const;
    long   getIntValue(const   std::string&   strSection, const   std::string&   strKey)   const;
    double getDoubleValue(const   std::string&   strSection, const   std::string&   strKey)   const;
    const char*  getValue(const   std::string&   strKey)   const;
    long   getIntValue(const   std::string&   strKey)   const;
    double getDoubleValue(const   std::string&   strKey)   const;
    void   printContent();

protected:
    bool   extractSection(char*   line, std::string&   strSection);
    bool   extractKeyValue(char*   line, std::pair<std::string, std::string>&   pairKeyValue);

private:
    FILE_CONTENT   m_content;

    char   m_chSectionBMark;
    char   m_chSectionEMark;
    char   m_chRecordEMark;
    char   m_chCommentMark;
    int     m_error_line;
};


#endif //AUDIOCLIENT_CCFGFILEPARSER_H
