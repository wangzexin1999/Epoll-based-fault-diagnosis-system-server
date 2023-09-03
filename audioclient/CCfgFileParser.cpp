//
// Created by 997289110 on 12/23/19.
//

#include <cstdio>
#include <cstring>
#include <fstream>
#include <cerrno>
#include <sstream>
#include <cstdlib>
#include <iostream>

#include   "CCfgFileParser.h"

using namespace std;

static inline bool istab(int c) { return (c == '\t'); }

static inline char *strltrim(char *str) {
    while (isspace(*str) || istab(*str)) {
        ++str;
    }
    return str;
}

static inline char *strrtrim(char *str) {
    int len = strlen(str) - 1;
    while (isspace(str[len]) || istab(str[len])) {
        str[len--] = '\0';
    }
    return str;
}

//////////////////////////////////////////////////////////////////////
//   Construction/Destruction
//////////////////////////////////////////////////////////////////////
int CCfgFileParser::parseFile(const char *lpszFilename) {
    using std::string;
    std::ifstream in1(lpszFilename);

    if (!in1.is_open()) return errno;

    char line[2048];
    char *pline;
    bool bInSection = false;
    SECTION_CONTENT *pSC = NULL;
    string strSection;
    std::pair<string, string> pairKeyValue;

    m_error_line = 0;
    m_content.clear();
    pSC = &m_content["default"];
    bInSection = true;
    string base64str("");
    string mystr("");
    while (!in1.eof()) {
        in1.getline(line, sizeof(line));

        if (line[strlen(line)] != '\n')
            mystr += line + string("\n");
        else
            mystr += line;

    }
    in1.close();

    stringstream in;
    in.str(mystr);

    while (!in.eof()) {    /*if(base64str.size() > sizeof(line))
		{
			printf("error in config parser!\n");
		        return -1;
		}
		memcpy(line, base64str.c_str(), base64str.size());
		*/
        in.getline(line, sizeof(line));
        pline = line;
        ++m_error_line;
        pline = strltrim(pline);
        if (pline[0] == '\0') continue;   //white   line,   skip
        if (pline[0] == m_chCommentMark) continue;   //comment   line,   skip
        if (bInSection) {
            //is   new-section   begin?
            if (pline[0] == m_chSectionBMark && extractSection(pline, strSection)) {
                pSC = &m_content[strSection];
            } else if (extractKeyValue(pline, pairKeyValue)) {
                //key-value   pair
                pSC->insert(pairKeyValue);
            } else {
                //in.close();
                return SYNTAX_ERROR;
            }
        } else {   //NOT   in   section
            //is   a   valid   section?
            if (extractSection(pline, strSection)) {
                pSC = &m_content[strSection];
                bInSection = true;
            } else {
                //in.close();
                return SYNTAX_ERROR;
            }
        }
    }
//	in.close();
    return 0;
}


const char *CCfgFileParser::getValue(const std::string &strSection,
                                     const std::string &strKey) const {
    FILE_CONTENT::const_iterator it;

    if ((it = m_content.find(strSection)) != m_content.end()) {
        SECTION_CONTENT::const_iterator p;
        const SECTION_CONTENT &section = (*it).second;//m_content[strSection];
        if ((p = section.find(strKey)) != section.end()) {
            return ((*p).second).c_str();
        }
    }
    printf("***ERROR cannot find the item : %s\n", strKey.c_str());
    return "";
}

long CCfgFileParser::getIntValue(const std::string &strSection,
                                 const std::string &strKey) const {
    return atol(getValue(strSection, strKey));
}

double CCfgFileParser::getDoubleValue(const std::string &strSection,
                                      const std::string &strKey) const {
    return atof(getValue(strSection, strKey));
}

const char *CCfgFileParser::getValue(const std::string &strKey) const {
    return getValue("default", strKey);
}

long CCfgFileParser::getIntValue(const std::string &strKey) const {
    return getIntValue("default", strKey);
}

double CCfgFileParser::getDoubleValue(const std::string &strKey) const {
    return getDoubleValue("default", strKey);
}

/*
 *     Description: Extract   section   name
 *     Parameters: line[IN]---A   string   line   to   be   parsed
 * strSection[OUT]---Section   name
 *
 *     Return   Value: if   section   name   is   in   the   line   return   true,or   return   false
 */
bool CCfgFileParser::extractSection(char *line, std::string &strSection) {
    char *tmp;
    if (line[0] == m_chSectionBMark) {
        if ((tmp = strchr(++line, m_chSectionEMark)) != NULL) {
            *tmp = '\0';
            strSection = line;
            return true;
        }
    }

    return false;
}


/*
 *     Description: Parse   a   record   line   into   a   std:pair   as   key   and   value
 *     Parameters: line[IN]---A   string   to   be   parsed
 * pairKeyValue[OUT]---Parsing   result
 *
 *     Return   Value: If   parse   successfully   return   true,or   return   false
 */

bool CCfgFileParser::extractKeyValue(char *line,
                                     std::pair<std::string, std::string> &pairKeyValue) {
    char *tmp;
    if ((tmp = strchr(line, m_chRecordEMark)) != NULL || (tmp = strchr(line, '\r')) != NULL ||
        (tmp = strchr(line, '='))) {
        if (*tmp == '=')
            tmp = line + strlen(line); //  tmp++;
        *tmp = '\0';   //ignore   content   after   ';'(the   RecordEMark)
        if ((tmp = strchr(line, '=')) != NULL) {
            *tmp++ = '\0';
            tmp = strltrim(tmp);
            tmp = strrtrim(tmp);
            line = strrtrim(line);

            pairKeyValue.first = line;
            pairKeyValue.second = tmp;
            return true;
        }
    }

    return false;
}

const char *CCfgFileParser::getErrorString(int err) {
    static char buf[100];
    if (err == SYNTAX_ERROR) {
        sprintf(buf, "configuration   file   format   is   invalid   at   line   %d", m_error_line);
        return buf;
    } else {
        return strerror(err);
    }
}


void CCfgFileParser::printContent() {
    using std::cout;
    using std::endl;
    FILE_CONTENT::const_iterator pf;
    SECTION_CONTENT::const_iterator ps;
    for (pf = m_content.begin(); pf != m_content.end(); ++pf) {
        cout << "section:" << (*pf).first << endl;
        const SECTION_CONTENT &sc = (*pf).second;
        for (ps = sc.begin(); ps != sc.end(); ++ps) {
            cout << '\t' << (*ps).first << "=" << (*ps).second << endl;
        }
    }
}
