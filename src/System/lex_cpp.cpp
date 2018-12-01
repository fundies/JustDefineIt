/**
 * @file lex_cpp.cpp
 * @brief Source implementing the C++ \c lexer class extensions.
 * 
 * This file's function will be referenced, directly or otherwise, by every
 * other function in the parser. The efficiency of its implementation is of
 * crucial importance. If this file runs slow, so do the others.
 * 
 * @section License
 * 
 * Copyright (C) 2011-2014 Josh Ventura
 * This file is part of JustDefineIt.
 * 
 * JustDefineIt is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 3 of the License, or (at your option) any later version.
 * 
 * JustDefineIt is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * JustDefineIt. If not, see <http://www.gnu.org/licenses/>.
**/

#include "lex_cpp.h"
#include <General/debug_macros.h>
#include <General/parse_basics.h>
#include <General/debug_macros.h>
#include <System/builtins.h>
#include <Parser/context_parser.h>
#include <API/context.h>
#include <API/AST.h>
#include <cstring>
#include <csignal>

#include <API/compile_settings.h>

using namespace jdi;
using namespace jdip;
using namespace std;

/// Returns whether s1 begins with s2, followed by whitespace.
static inline bool strbw(const char* s1, const char (&s2)[3]) { return *s1 == *s2 and s1[1] == s2[1] and (!is_letterd(s1[2])); }
static inline bool strbw(const char* s1, const char (&s2)[4]) { return *s1 == *s2 and s1[1] == s2[1] and s1[2] == s2[2] and (!is_letterd(s1[3])); }
static inline bool strbw(const char* s1, const char (&s2)[5]) { return *s1 == *s2 and s1[1] == s2[1] and s1[2] == s2[2] and s1[3] == s2[3] and (!is_letterd(s1[4])); }
static inline bool strbw(const char* s1, const char (&s2)[6]) { return *s1 == *s2 and s1[1] == s2[1] and s1[2] == s2[2] and s1[3] == s2[3] and s1[4] == s2[4] and (!is_letterd(s1[5])); }
static inline bool strbw(const char* s1, const char (&s2)[7]) { return *s1 == *s2 and s1[1] == s2[1] and s1[2] == s2[2] and s1[3] == s2[3] and s1[4] == s2[4] and s1[5] == s2[5] and (!is_letterd(s1[6])); }
static inline bool strbw(const char* s1, const char (&s2)[11]){ return *s1 == *s2 and s1[1] == s2[1] and s1[2] == s2[2] and s1[3] == s2[3] and s1[4] == s2[4] and s1[5] == s2[5] and s1[6] == s2[6] and s1[7] == s2[7] and s1[8] == s2[8] and s1[9] == s2[9] and (!is_letterd(s1[10])); }
static inline bool strbw(char s) { return !is_letterd(s); }

#if 0
void lexer_cpp::skip_comment()
{
  #if ALLOW_MULTILINE_COMMENTS
  while (++pos < length and cfile[pos] != '\n' and cfile[pos] != '\r') if (cfile[pos] == '\\') {
    if (cfile[++pos] == '\n') ++line, lpos = pos;
    else if (cfile[pos] == '\r') pos += cfile[pos+1] == '\n', ++line, lpos = pos;
  }
  #else
  while (++pos < length and cfile[pos] != '\n' and cfile[pos] != '\r');
  #endif
}

inline void lexer_cpp::skip_multiline_comment()
{
  char pchr = 0;
  pos += 2; // Skip two chars so we don't break on /*/
  do if (pos >= length) return;
    else if (cfile[pos] == '\n' or (cfile[pos] == '\r' and cfile[pos+1] != '\n')) ++line, lpos = pos;
    while ((pchr = cfile[pos++]) != '*' or cfile[pos] != '/');
  ++pos;
}

void lexer_cpp::skip_string(error_handler *herr)
{
  register const char endc = cfile[pos];
  while (++pos < length and cfile[pos] != endc)
  {
    if (cfile[pos] == '\\') {
      if (cfile[++pos] == '\n') ++line, lpos = pos;
      else if (cfile[pos] == '\r') {
        if (cfile[++pos] != '\n') --pos;
        ++line, lpos = pos; 
      }
    }
    else if (cfile[pos] == '\n' or cfile[pos] == '\r') {
      herr->error("Unterminated string literal", filename, line, pos-lpos);
      break;
    }
  }
  if (cfile[pos] != endc)
    herr->error("Unterminated string literal", filename, line, pos-lpos);
}

static inline void skip_string(const char* cfile, size_t &pos, size_t length)
{
  register const char endc = cfile[pos];
  while (++pos < length and cfile[pos] != endc)
    if (cfile[pos] == '\\') if (cfile[pos++] == '\r' and cfile[pos] == '\n') ++pos;
}

void lexer_cpp_base::skip_whitespace()
{
  while (pos < length) {
    while (is_spacer(cfile[pos])) if (++pos >= length) return;
    if (cfile[pos] == '\n' or (cfile[pos] == '\r' and cfile[pos+1] != '\n')) { ++line; lpos = pos++; continue; }
    if (cfile[pos] == '/') {
      if (cfile[++pos] == '/') { skip_comment(); continue; }
      if (cfile[pos] == '*') { skip_multiline_comment(); continue; }
      return;
    }
    return;
  }
}
#endif

void lexer_cpp::enter_macro(macro_type* macro) {
  if (macro->value.empty()) return;
  open_macros.push_back({macro->name, cfile.lnum, cfile.lpos, &macro->value});
}

static inline void lex_error(error_handler *herr, const llreader &cfile, string_view msg) {
  herr->error(msg, cfile.name, cfile.lnum, cfile.pos - cfile.lpos);
}

static inline void skip_string(llreader &cfile, error_handler *herr) {
  const char endc = cfile.at();
  while (cfile.next() != EOF && cfile.at() != endc) {
    if (cfile.at() == '\\') {
      if (cfile.next() == EOF) {
        lex_error(herr, cfile, "You can't escape the file ending, jackwagon.");
        return;
      } else if (cfile.at() == '\n') {
        ++cfile.lnum, cfile.lpos = cfile.pos;
      } else if (cfile.at() == '\r') {
        if (cfile.next() != '\n') --cfile.pos;
        ++cfile.lnum, cfile.lpos = cfile.pos; 
      }
    }
    else if (cfile.at() == '\n' or cfile.at() == '\r') {
      lex_error(herr, cfile, "Unterminated string literal");
      break;
    }
  }
  if (cfile.eof() or cfile.at() != endc)
    lex_error(herr, cfile, "Unterminated string literal");
}

bool lexer_cpp::parse_macro_params(const macro_type* mf, vector<string> *out) {
  cfile.skip_whitespace();
  size_t &pos = cfile.pos;
  
  size_t pspos = ++pos;
  vector<string> dest;
  dest.reserve(mf->args.size());
  
  // Read the parameters into our argument vector
  int nestcnt = 1;
  while (pos < cfile.length && nestcnt > 0) {
    if (cfile[pos] == ',' and nestcnt == 1) {
      dest.push_back(string(cfile+pspos, pos-pspos));
      pspos = ++pos;
      cfile.skip_whitespace();
      continue;
    } else if (cfile[pos] == ')') { if (--nestcnt) ++pos; continue; }
      else if (cfile[pos] == '(') ++nestcnt;
      else if (cfile[pos] == '"' or cfile[pos] == '\'') 
        skip_string(cfile, herr);
    ++pos; 
    cfile.skip_whitespace();
  }
  if (cfile.eof() or cfile.at() != ')') {
    lex_error(herr, cfile, "Unterminated parameters to macro function");
    return false;
  }
  
  if (pos > pspos) // If there was a parameter
    dest.push_back(string(cfile+pspos, pos-pspos)); // FIXME: XXX: Same as above.
    //dest.push_back(_flatten(string(cfile+pspos, pos-pspos), macros, token_t(token_basics(TT_INVALID, filename, line, pos)), herr)); // Nab the final parameter
  
  ++pos; // Consume closing parenthesis to argument list
  out->swap(dest);
  return true;
}

bool lexer_cpp::parse_macro_function(const macro_type* mf, error_handler *herr) {
  bool already_open = false; // Test if we're in this macro already
  vector<openfile>::iterator it = files.begin();
  for (size_t i = 0; i < open_macros.size(); ++i, --it) {
    if (it->name != mf->name) continue;
    already_open = true;
    break;
  }
  if (already_open)
    return true;
  
  size_t spos = cfile.pos, slpos = cfile.lpos, sline = cfile.lnum;
  cfile.skip_whitespace(); // Move to the next "token"
  if (cfile.eof() or cfile.at() != '(') {
    cfile.pos = spos, cfile.lpos = slpos, cfile.lnum = sline;
    return false;
  }
  
  vector<string> params;
  if (!parse_macro_params(mf, cfile, &params, herr)) return false;
  vector<token_t> tokens = mf->substitute_and_unroll(params, herr)
  open_macros.push_back({mf->name, line, lpos, std::move(tokens)});
  return true;
}

string lexer_cpp::read_preprocessor_args(error_handler *herr)
{
  for (;;) {
    while (cfile[pos] == ' ' or cfile[pos] == '\t') if (++pos >= length) return "";
    if (cfile[pos] == '/') {
      if (cfile[++pos] == '/') { skip_comment(); return ""; }
      if (cfile[pos] == '*') { skip_multiline_comment(); continue; }
      break;
    }
    if (cfile[pos] == '\n' or cfile[pos] == '\r') return "";
    if (cfile[pos] == '\\' and (cfile[++pos] == '\n' or (cfile[pos] == '\r' and (cfile[++pos] == '\n' or --pos)))) { lpos = pos++; ++line; }
    break;
  }
  string res;
  res.reserve(256);
  size_t spos = pos;
  while (pos < length and cfile[pos] != '\n' and cfile[pos] != '\r') {
    if (cfile[pos] == '/') {
      if (cfile[++pos] == '/') { res += string(cfile+spos,pos-spos-1); skip_comment(); return res; }
      if (cfile[pos] == '*') {
        res.reserve(res.length()+pos-spos);
        res += string(cfile+spos,pos-spos-1), res += " ";
        skip_multiline_comment(); spos = pos; continue; }
    }
    if (cfile[pos] == '\'' or cfile[pos] == '"') skip_string(herr), ++pos;
    else if (cfile[pos] == '\\' and (cfile[++pos] == '\n' or (cfile[pos] == '\r' and (cfile[++pos] == '\n' or (--pos, true))))) { lpos = pos++; ++line; }
    else ++pos;
  }
  res += string(cfile+spos,pos-spos);
  {
    register size_t trim = res.length() - 1;
    if (is_useless(res[trim])) {
      while (is_useless(res[--trim]));
      res.erase(++trim);
    }
  }
  return res;
}

#ifdef DEBUG_MODE
/// This function will be passed signals
static void donothing(int) {}
#endif

// Optional AST rendering
#include <General/debug_macros.h>

/**
  @section Implementation
  
  For the preprocessor's implementation, we switch gears a bit. The preprocessor needs
  to be the fastest aspect of the entire parser, as directives are everywhere. As such,
  it operates on a hacked-up switch statement using a whole heap of gotos, designed to
  support a specific set of macros. Inserting a new directive into this switch is still
  simple enough, but unlike other aspects of the parser, is not as trivial as a single
  function call. Don't gripe; originally I was going to use a perfect hash.
**/
void lexer_cpp::handle_preprocessor(error_handler *herr)
{
  top:
  bool variadic = false; // Whether this function is variadic
  while (cfile[pos] == ' ' or cfile[pos] == '\t') ++pos;
  const size_t pspos = pos;
  switch (cfile[pos++])
  {
    case 'd':
      if (strbw(cfile+pos, "efine")) { pos += 5; goto case_define; }
      goto failout;
    case 'e':
      if (cfile[pos] == 'n') { if (strbw(cfile+pos+1, "dif")) { pos += 4; goto case_endif; } goto failout; }
      if (cfile[pos] == 'l')
      { 
        if (cfile[++pos] == 's') { if (cfile[++pos] == 'e') { ++pos; goto case_else; } goto failout; }
        if (cfile[pos] == 'i' and cfile[++pos] == 'f')
        {
          if (strbw(cfile[++pos])) goto case_elif;
          if (cfile[pos] == 'd') { if (strbw(cfile+pos+1, "ef"))  { pos += 3; goto case_elifdef;  } goto failout; }
          if (cfile[pos] == 'n') { if (strbw(cfile+pos+1, "def")) { pos += 4; goto case_elifndef; } goto failout; }
        }
        goto failout;
      }
      if (strbw(cfile+pos, "rror")) { pos += 4; goto case_error; }
      goto failout;
    case 'i':
      if (cfile[pos] == 'f')
      {
        if (strbw(cfile[++pos])) goto case_if;
        if (cfile[pos] == 'd') { if (strbw(cfile+pos+1, "ef"))  { pos += 3; goto case_ifdef;  } goto failout; }
        if (cfile[pos] == 'n') { if (strbw(cfile+pos+1, "def")) { pos += 4; goto case_ifndef; } goto failout; }
        goto failout;
      }
      if (cfile[pos] == 'n') {
        if (strbw(cfile+pos+1, "clude")) { pos += 6; goto case_include; }
        if (strbw(cfile+pos+1, "clude_next")) { pos += 11; goto case_include_next; }
        goto failout;
      }
      if (cfile[pos] == 'm') { if (strbw(cfile+pos+1, "port"))  { pos += 5; goto case_import;  } goto failout; }
      goto failout;
    case 'l':
      if (strbw(cfile+pos, "ine")) { pos += 3; goto case_line; }
      goto failout;
    case 'p':
      if (strbw(cfile+pos, "ragma")) { pos += 5; goto case_pragma; }
      goto failout;
    case 'u':
      if (strbw(cfile+pos, "ndef")) { pos += 4; goto case_undef; }
      if (strbw(cfile+pos, "sing")) { pos += 4; goto case_using; }
      goto failout;
    case 'w':
      if (strbw(cfile+pos, "arning")) { pos += 6; goto case_warning; }
      goto failout;
    default: goto failout;
  }
  
  while (false) {  // This is an improvised switch statement; allows "break" to work.
    case_define: {
      string argstrs = read_preprocessor_args(herr);
      const char* argstr = argstrs.c_str();
      if (!conditionals.empty() and !conditionals.top().is_true)
        break;
      size_t i = 0;
      while (is_useless(argstr[i])) ++i;
      if (!is_letter(argstr[i])) {
        herr->error("Expected macro definiendum at this point", filename, line, pos-lpos);
      }
      const size_t nsi = i;
      while (is_letterd(argstr[++i]));
      pair<macro_iter, bool> mins = macros.insert(pair<string,macro_type*>(argstrs.substr(nsi,i-nsi),NULL));
      
      if (argstr[i] == '(') {
        vector<string> paramlist;
        while (is_useless(argstr[++i]));
        if (argstr[i] != ')') for (;;) {
          if (!is_letter(argstr[i])) {
            if (argstr[i] == '.' and argstr[i+1] == '.' and argstr[i+2] == '.') {
              variadic = true, i += 3;
              while (is_useless(argstr[i])) ++i;
              if (argstr[i] != ')')
                herr->error("Expected end of parameters after variadic", filename, line, pos-lpos);
              break;
            }
            else {
              herr->error("Expected parameter name for macro declaration", filename, line, pos-lpos);
              break;
            }
          }
          const size_t si = i;
          while (is_letterd(argstr[++i]));
          paramlist.push_back(argstrs.substr(si, i-si));
          
          while (is_useless(argstr[i])) ++i;
          if (argstr[i] == ')') break;
          if (argstr[i] == ',') { while (is_useless(argstr[++i])); continue; }
          
          // Handle variadic macros (if we are at ...)
          if (argstr[i] == '.' and argstr[i+1] == '.' and argstr[i+2] == '.') {
            i += 2; while (is_useless(argstr[++i]));
            variadic = true;
            if (argstr[i] == ')') break;
            herr->error("Expected closing parenthesis at this point; further parameters not allowed following variadic", filename, line, pos-lpos);
          }
          else
            herr->error("Expected comma or closing parenthesis at this point", filename, line, pos-lpos);
        }
        
        if (!mins.second) { // If no insertion was made; ie, the macro existed already.
        //  if ((size_t)mins.first->second->argc != paramlist.size())
        //    herr->warning("Redeclaring macro function `" + mins.first->first + '\'', filename, line, pos-lpos);
          macro_type::free(mins.first->second);
          mins.first->second = NULL;
        }
        mins.first->second = new macro_function(mins.first->first,paramlist, argstrs.substr(++i), variadic, herr);
      }
      else
      {
        if (!mins.second) { // If no insertion was made; ie, the macro existed already.
        //  if (mins.first->second->argc != -1 or (mins.first->second)->value != argstr.substr(i))
        //    herr->warning("Redeclaring macro `" + mins.first->first + '\'', filename, line, pos-lpos);
          macro_type::free(mins.first->second);
          mins.first->second = NULL;
        }
        while (is_useless(argstr[i])) ++i;
        mins.first->second = new macro_scalar(mins.first->first,argstrs.substr(i));
      }
    } break;
    case_error: {
        string emsg = read_preprocessor_args(herr);
        if (conditionals.empty() or conditionals.top().is_true)
          herr->error(token_basics("#error " + emsg,filename,line,pos-lpos));
      } break;
      break;
    case_elif:
        if (conditionals.empty())
          herr->error(token_basics("Unexpected #elif directive; no matching #if",filename,line,pos-lpos));
        else {
          if (conditionals.top().is_true)
            conditionals.top().is_true = conditionals.top().can_be_true = false;
          else {
            if (conditionals.top().can_be_true) {
              conditionals.pop();
              goto case_if;
            }
          }
        }
      break;
    case_elifdef:
        if (conditionals.empty())
          herr->error(token_basics("Unexpected #elifdef directive; no matching #if",filename,line,pos-lpos));
        else {
          if (conditionals.top().is_true)
            conditionals.top().is_true = conditionals.top().can_be_true = false;
          else {
            if (conditionals.top().can_be_true) {
              conditionals.pop();
              goto case_ifdef;
            }
          }
        }
      break;
    case_elifndef:
        if (conditionals.empty())
          herr->error(token_basics("Unexpected #elifndef directive; no matching #if",filename,line,pos-lpos));
        else {
          if (conditionals.top().is_true)
            conditionals.top().is_true = conditionals.top().can_be_true = false;
          else {
            if (conditionals.top().can_be_true) {
              conditionals.pop();
              goto case_ifndef;
            }
          }
        }
      break;
    case_else:
        if (conditionals.empty())
          herr->error(token_basics("Unexpected #else directive; no matching #if",filename,line,pos-lpos));
        else {
          if (conditionals.top().is_true)
            conditionals.top().is_true = conditionals.top().can_be_true = false;
          else
            conditionals.top().is_true = conditionals.top().can_be_true;
        }
      break;
    case_endif:
        if (conditionals.empty())
          return herr->error(token_basics("Unexpected #endif directive: no open conditionals.",filename,line,pos-lpos));
        conditionals.pop();
      break;
    case_if: 
        if (conditionals.empty() or conditionals.top().is_true) {
          AST a;
          if (mctex->get_AST_builder()->parse_expression(&a) or !a.eval(error_context(herr, filename, line, pos-lpos))) {
            //token_t res;
            render_ast(a, "if_directives");
            conditionals.push(condition(0,1));
            break;
          }
          render_ast(a, "if_directives");
          conditionals.push(condition(1,0));
        }
        else
          conditionals.push(condition(0,0));
      break;
    case_ifdef: {
        while (is_useless(cfile[pos])) ++pos;
        if (!is_letter(cfile[pos])) {
          herr->error("Expected identifier to check against macros",filename,line,pos);
          break;
        }
        const size_t msp = pos;
        while (is_letterd(cfile[++pos]));
        string macro(cfile+msp, pos-msp);
        if (conditionals.empty() or conditionals.top().is_true) {
          if (macros.find(macro) == macros.end()) {
            token_t res;
            conditionals.push(condition(0,1));
            break;
          }
          conditionals.push(condition(1,0));
        }
        else
          conditionals.push(condition(0,0));
      } break;
    case_ifndef: {
        while (is_useless(cfile[pos])) ++pos;
        if (!is_letter(cfile[pos])) {
          herr->error("Expected identifier to check against macros",filename,line,pos);
          break;
        }
        const size_t msp = pos;
        while (is_letterd(cfile[++pos]));
        string macro(cfile+msp, pos-msp);
        if (conditionals.empty() or conditionals.top().is_true) {
          if (macros.find(macro) != macros.end()) {
            token_t res;
            conditionals.push(condition(0,1));
            break;
          }
          conditionals.push(condition(1,0));
        }
        else
          conditionals.push(condition(0,0));
      } break;
    case_import:
      break;
    case_include: {
        bool incnext;
        if (true) incnext = false;
        else { case_include_next: incnext = true; }
        
        string fnfind = read_preprocessor_args(herr);
        if (!conditionals.empty() and !conditionals.top().is_true)
      break;
        
        bool chklocal = false;
        char match = '>';
        if (!incnext and fnfind[0] == '"')
          chklocal = true, match = '"';
        else if (fnfind[0] != '<') {
          herr->error("Expected filename inside <> or \"\" delimiters", filename, line, pos - lpos);
          break;
        }
        fnfind[0] = '/';
        for (size_t i = 0; i < fnfind.length(); ++i)
          if (fnfind[i] == match) { fnfind.erase(i); break; }
        
        if (files.size() > 9000) {
          herr->error("Nested include count is OVER NINE THOUSAAAAAAAAND. Not including another.");
          break;
        }
        
        string incfn, fdir = sdir;
        llreader incfile;
        if (chklocal)
          incfile.open((incfn = path + fnfind).c_str());
        for (size_t i = 0; i < builtin->search_dir_count(); ++i) {
          if (incfile.is_open()) break;
          if (!incnext) {
            // cout << "fnfind:" << endl;
            // cout << "  [" << fnfind.length() << "]\"" << fnfind << '"' << endl;
            // cout << "  [" << builtin->search_dir(i).length() << "]\"" << builtin->search_dir(i) << '"' << endl;
            // cout << "  cat: " << flush << (builtin->search_dir(i) + fnfind) << endl;
            incfile.open((incfn = (fdir = builtin->search_dir(i)) + fnfind).c_str());
          }
          else
            incnext = sdir != builtin->search_dir(i);
        }
        if (!incfile.is_open()) {
          herr->error("Could not find " + fnfind.substr(1), filename, line, pos-lpos);
          if (chklocal) cerr << "  Checked " << path << endl;
          for (size_t i = 0; !incfile.is_open() and i < builtin->search_dir_count(); ++i)
            cerr << "  Checked " << builtin->search_dir(i) << endl;
          break;
        }
        
        openfile of(filename, sdir = fdir, line, lpos, *this);
        files.enswap(of);
        pair<set<string>::iterator, bool> fi = visited_files.insert(incfn);
        filename = fi.first->c_str();
        this->consume(incfile);
        line = 1;
      } break;
    case_line:
      break;
    case_pragma:
        #ifdef DEBUG_MODE
        {
          string n = read_preprocessor_args(herr);
          if (n == "DEBUG_ENTRY_POINT" and (conditionals.empty() or conditionals.top().is_true)) {
            signal(SIGTRAP, donothing); // Try not to die when we raise hell in the interrupt handler briefly
            asm("INT3;"); // Raise hell in the interrupt handler; the debugger will grab us from here
            cout << "* Debug entry point" << endl;
          }
        }
        #else
          read_preprocessor_args(herr);
        #endif
      break;
    case_undef:
        if (!conditionals.empty() and !conditionals.top().is_true)
          break;
        
        while (is_useless(cfile[pos])) ++pos;
        if (!is_letter(cfile[pos]))
          herr->error("Expected macro identifier at this point", filename, line, pos);
        else {
          const size_t nspos = pos;
          while (is_letterd(cfile[++pos]));
          macro_iter mdel = macros.find(string(cfile+nspos,pos-nspos));
          if (mdel != macros.end()) {
            macro_type::free(mdel->second);
            macros.erase(mdel);
          }
        }
      break;
    case_using:
      break;
    case_warning: {
        string wmsg = read_preprocessor_args(herr);
        if (conditionals.empty() or conditionals.top().is_true)
          herr->warning(token_basics("#warning " + wmsg,filename,line,pos-lpos));
      } break;
  }
  if (conditionals.empty() or conditionals.top().is_true)
    return;
  
  // skip_to_macro:
  while (pos < length) {
    if (cfile[pos] == '/' || is_useless(cfile[pos]))
      skip_whitespace();
    else if (cfile[pos++] == '#')
      goto top;
  }
  herr->error("Expected closing preprocessors before end of code",filename,line,pos-lpos);
  return;
  
  failout:
    while (is_letterd(cfile[pos])) ++pos;
    string ppname(cfile + pspos, pos - pspos);
    herr->error(token_basics("Invalid preprocessor directive `" + ppname + "'",filename,line,pos-lpos));
    while (pos < length and cfile[pos] != '\n' and cfile[pos] != '\r') ++pos;
}

#include <cstdio>
token_t lexer_cpp_base::read_raw(error_handler *herr)
{
  #ifdef DEBUG_MODE
    static int number_of_times_GDB_has_dropped_its_ass = 0;
    ++number_of_times_GDB_has_dropped_its_ass;
  #endif
  
  for (;;) {  // Loop until we find something or hit world's end
    if (pos >= length) return token_t(token_basics(TT_ENDOFCODE,filename, line, pos - lpos));
    
    // Skip all whitespace
    while (is_useless(cfile[pos])) {
      if (cfile[pos] == '\n') {
        ++line, lpos = pos++;
        return token_t(token_basics(TTM_NEWLINE, filename, line, pos - lpos), cfile + lpos, 1);
      }
      if (cfile[pos] == '\r') {
        ++line; lpos = pos;
        if (cfile[++pos] != '\n') ++pos;
        return token_t(token_basics(TTM_NEWLINE, filename, line, pos - lpos),
                       cfile + lpos, pos - lpos);
      }
      if (++pos >= length) return token_t(token_basics(TT_ENDOFCODE,filename, line, pos - lpos));
    }
    
    //==============================================================================================
    //====: Check for and handle comments. :========================================================
    //==============================================================================================
    
    const size_t spos = pos;
    switch (cfile[pos++])
    {
    // Skip all whitespace
    
     case '/': {
      if (cfile[pos] == '*') { skip_multiline_comment(); continue; }
      if (cfile[pos] == '/') { skip_comment(); continue; }
      if (cfile[pos] == '=') {
        ++pos;
        return token_t(token_basics(TT_DIVIDE_ASSIGN, filename, line, pos - lpos),
                       cfile + pos - 2, 2);
      }
      return token_t(token_basics(TT_SLASH, filename, line, pos - lpos), cfile + pos - 1, 1);
    }
      
      
    default:
    //==============================================================================================
    //====: Not at a comment. See if we're at an identifier. :======================================
    //==============================================================================================
    
    if (is_letter(cfile[spos])) {  // Check if we're at an identifier or keyword.
      while (pos < length and is_letterd(cfile[pos])) ++pos;
      if (cfile[spos] == 'L' and pos - spos == 1 and cfile[pos] == '\'') {
        skip_string(herr);
        return token_t(token_basics(TT_CHARLITERAL, filename, line, spos - lpos),
                       cfile + spos, ++pos - spos);
      }
      return token_t(token_basics(TT_IDENTIFIER,filename,line,spos-lpos), cfile + spos, pos - spos);
    }
    
    goto unknown;
    
    //============================================================================================
    //====: Not at an identifier. Maybe at a number? :============================================
    //============================================================================================
    
    case '0': { // Check if the number is hexadecimal or octal.
      if (cfile[pos] == 'x') { // Check if the number is hexadecimal.
        // Yes, it is hexadecimal.
        const size_t sp = pos;
        while (++pos < length and is_hexdigit(cfile[pos]));
        while (pos < length and is_letter(cfile[pos])) pos++; // Include the flags, like ull
        return token_t(token_basics(TT_HEXLITERAL,filename,line,pos-lpos), cfile+sp, pos-sp);
      }
      // Turns out, it's octal.
      const size_t sp = --pos;
      while (++pos < length and is_hexdigit(cfile[pos]));
      while (pos < length and is_letter(cfile[pos])) pos++; // Include the flags, like ull
      return token_t(token_basics(TT_OCTLITERAL,filename,line,pos-lpos), cfile+sp, pos-sp);
    }
    
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8':
    case '9': {
      // Turns out, it's decimal.
      handle_decimal:
      while (pos < length and is_digit(cfile[pos])) ++pos;
      if (cfile[pos] == '.')
        while (++pos < length and is_digit(cfile[pos]));
      if (cfile[pos] == 'e' or cfile[pos] == 'E') { // Accept exponents
        if (cfile[++pos] == '-') ++pos;
        while (pos < length and is_digit(cfile[pos])) ++pos;
      }
      while (pos < length and is_letter(cfile[pos])) ++pos; // Include the flags, like ull
      return token_t(token_basics(TT_DECLITERAL,filename,line,pos-lpos), cfile+spos, pos-spos);
    }
    
    //==============================================================================================
    //====: Not at a number. Find out where we are. :===============================================
    //==============================================================================================
    
      case ';':
        return token_t(token_basics(TT_SEMICOLON,filename,line,spos-lpos));
      case ',':
        return token_t(token_basics(TT_COMMA,filename,line,spos-lpos));
      case '+':
        if (cfile[pos] == '+') {
          ++pos;
          return token_t(token_basics(TT_INCREMENT, filename, line, spos - lpos), cfile + spos, 2);
        }
        if (cfile[pos] == '=') {
          ++pos;
          return token_t(token_basics(TT_ADD_ASSIGN, filename, line, spos - lpos), cfile + spos, 2);
        }
        return token_t(token_basics(TT_ADD_ASSIGN, filename, line, spos - lpos), cfile + spos, 1);
      case '-':
        if (cfile[pos] == '+') {
          ++pos;
          return token_t(token_basics(TT_INCREMENT, filename, line, spos - lpos), cfile + spos, 2);
        }
        if (cfile[pos] == '=') {
          ++pos;
          return token_t(token_basics(TT_ADD_ASSIGN, filename, line, spos - lpos), cfile + spos, 2);
        }
        if (cfile[pos] == '>') {
          ++pos;
          return token_t(token_basics(TT_ARROW, filename, line, spos - lpos), cfile + spos, 2);
        }
        return token_t(token_basics(TT_ADD_ASSIGN, filename, line, spos - lpos), cfile + spos, 1);
      case '=':
        if (cfile[pos] == cfile[spos]) {
          ++pos;
          return token_t(token_basics(TT_EQUAL_TO, filename, line, spos - lpos), cfile + spos, 2);
        }
        return token_t(token_basics(TT_EQUAL, filename, line, spos - lpos), cfile + spos, 1);
      case '&':
        if (cfile[pos] == '&') {
          ++pos;
          return token_t(token_basics(TT_AMPERSANDS, filename, line, spos - lpos), cfile + spos, 2);
        }
        if (cfile[pos] == '=') {
          ++pos;
          return token_t(token_basics(TT_AND_ASSIGN, filename, line, spos - lpos), cfile + spos, 2);
        }
        return token_t(token_basics(TT_AMPERSAND, filename, line, spos - lpos), cfile + spos, 1);
      case '|':
        if (cfile[pos] == '|') {
          ++pos;
          return token_t(token_basics(TT_PIPES, filename, line, spos - lpos), cfile + spos, 2);
        }
        if (cfile[pos] == '=') {
          ++pos;
          return token_t(token_basics(TT_OR_ASSIGN, filename, line, spos - lpos), cfile + spos, 2);
        }
        return token_t(token_basics(TT_PIPE, filename, line, spos - lpos), cfile + spos, 1);
      case '~':
        if (cfile[pos] == '=') {
          ++pos;
          return token_t(token_basics(TT_NEGATE_ASSIGN, filename, line, spos - lpos),
                         cfile + spos, 2);
        }
        return token_t(token_basics(TT_TILDE, filename, line, spos - lpos), cfile + spos, 1);
      case '!':
        if (cfile[pos] == '=') {
          ++pos;
          return token_t(token_basics(TT_NOT_EQUAL_TO, filename, line, spos - lpos),
                         cfile + spos, 2);
        }
        return token_t(token_basics(TT_NOT, filename, line, spos - lpos), cfile + spos, 1);
      case '%':
        if (cfile[pos] == '=') {
          ++pos;
          return token_t(token_basics(TT_MODULO_ASSIGN, filename, line, spos - lpos),
                         cfile + spos, 2);
        }
        return token_t(token_basics(TT_MODULO, filename, line, spos - lpos), cfile + spos, 1);
      case '*':
        if (cfile[pos] == '=') {
          ++pos;
          return token_t(token_basics(TT_MULTIPLY_ASSIGN, filename, line, spos - lpos),
                         cfile + spos, 2);
        }
        return token_t(token_basics(TT_STAR, filename, line, spos - lpos), cfile + spos, 1);
      case '^':
        if (cfile[pos] == '=') {
          ++pos;
          return token_t(token_basics(TT_XOR_ASSIGN, filename, line, spos - lpos), cfile + spos, 2);
        }
        return token_t(token_basics(TT_NOT, filename, line, spos - lpos), cfile + spos, 1);
      case '>':
        if (cfile[pos] == '>') {
          ++pos;
          return token_t(token_basics(TT_RSHIFT, filename, line, spos - lpos), cfile + spos, 2);
        }
        if (cfile[pos] == '=') {
          ++pos;
          return token_t(token_basics(TT_GREATER_EQUAL, filename, line, spos - lpos), cfile + spos, 2);
        }
        return token_t(token_basics(TT_GREATERTHAN, filename, line, spos - lpos), cfile + spos, 1);
      case '<':
        if (cfile[pos] == '<') {
          ++pos;
          return token_t(token_basics(TT_LSHIFT, filename, line, spos - lpos), cfile + spos, 2);
        }
        if (cfile[pos] == '=') {
          ++pos;
          return token_t(token_basics(TT_LESS_EQUAL, filename, line, spos - lpos), cfile + spos, 2);
        }
        return token_t(token_basics(TT_LESSTHAN, filename, line, spos - lpos), cfile + spos, 1);
      case ':':
        pos += cfile[pos] == cfile[spos];
        return token_t(token_basics(pos - spos == 1 ? TT_COLON : TT_SCOPE,
                                    filename, line, spos - lpos), cfile + spos, 1);
      case '?':
        return token_t(token_basics(TT_QUESTIONMARK, filename, line, spos - lpos), cfile + spos, 1);
      
      case '.':
          if (is_digit(cfile[pos]))
            goto handle_decimal;
          else if (cfile[pos] == '.') {
            if (cfile[++pos] == '.') {
              ++pos;
              return token_t(token_basics(TT_ELLIPSIS, filename, line, spos - lpos),
                             cfile + spos, 3);
            }
            --pos;
          }
          if (cfile[pos] == '*') {
            ++pos;
            return token_t(token_basics(TT_DOT_STAR,filename,line,spos-lpos), cfile+spos, 1);
          }
        return token_t(token_basics(TT_DOT,filename,line,spos-lpos), cfile+spos, 1);
      
      case '(': return token_t(token_basics(TT_LEFTPARENTH, filename,line,spos-lpos));
      case '[': return token_t(token_basics(TT_LEFTBRACKET, filename,line,spos-lpos));
      case '{': return token_t(token_basics(TT_LEFTBRACE,   filename,line,spos-lpos));
      case '}': return token_t(token_basics(TT_RIGHTBRACE,  filename,line,spos-lpos));
      case ']': return token_t(token_basics(TT_RIGHTBRACKET,filename,line,spos-lpos));
      case ')': return token_t(token_basics(TT_RIGHTPARENTH,filename,line,spos-lpos));
      
      case '#':
        if (cfile[pos] == '#') {
          ++pos;
          return token_t(token_basics(TTM_CONCAT, filename, line, 2));
        }
        return token_t(token_basics(TTM_TOSTRING, filename, line, 1));
      
      case '\\':
        if (cfile[pos] != '\n') {
          if (cfile[pos] != '\r')
            herr->error("Stray backslash", filename, line, pos-lpos);
          else if (cfile[++pos] != '\n')
            --pos;
        }
        ++line;
        lpos = pos++;
        continue;
      
      case '"': {
        --pos; skip_string(herr);
        return token_t(token_basics(TT_STRINGLITERAL,filename,line,spos-lpos), cfile + spos, ++pos-spos);
      }
      
      case '\'': {
        --pos; skip_string(herr);
        return token_t(token_basics(TT_CHARLITERAL,filename,line,spos-lpos), cfile + spos, ++pos-spos);
      }
      
      unknown: {
        char errbuf[320];
        sprintf(errbuf, "Unrecognized symbol (char)0x%02X '%c'", (int)cfile[spos], cfile[spos]);
        herr->error(errbuf);
        return token_t(token_basics(TT_INVALID,filename,line,pos-lpos++));
      }
    }
  }
  
  cerr << "UNREACHABLE BLOCK REACHED" << endl;
  return token_t(TT_INVALID,filename,line,pos-lpos++);
}

token_t lexer_cpp::get_token(error_handler *herr)
{
  token_t res;
  top:
  do res = read_raw(herr); while (res.type == TTM_NEWLINE);
  if (res.type == TT_IDENTIFIER) {
    string fn = res.content.toString();
    macro_iter mi;
    
    mi = macros.find(fn);
    if (mi != macros.end()) {
      if (mi->second->argc < 0) {
        bool already_open = false; // Test if we're in this macro already
        quick::stack<openfile>::iterator it = files.begin();
        for (unsigned i = 0; i < open_macro_count; ++i)
          if (it->filename == fn) { already_open = true; break; }
          else --it;
        if (!already_open) {
          enter_macro( mi->second);
          goto top;
        }
      }
      else {
        if (parse_macro_function((macro_function*) mi->second, herr))
          goto top;
      }
    }
    
    keyword_map::iterator kwit = keywords.find(fn);
    if (kwit != keywords.end()) {
      if (kwit->second == TT_INVALID) {
        mi = kludge_map.find(fn);
        #ifdef DEBUG_MODE
        if (mi == kludge_map.end())
          cerr << "SYSTEM ERROR! KEYWORD `" << fn << "' IS DEFINED AS INVALID" << endl;
        #endif
        if (mi->second->argc < 0)
          enter_macro(mi->second);
        else if (!parse_macro_function((macro_function*)mi->second, herr))
          return res;
        goto top;
      }
      res.type = kwit->second;
      return res;
    }
    
    tf_iter tfit = builtin_declarators.find(fn);
    if (tfit != builtin_declarators.end()) {
      if (tfit->second->usage  & UF_PRIMITIVE)
        res.type = TT_DECLARATOR, res.def = tfit->second->def;
      else
        res.type = TT_DECFLAG, res.def = (definition*)tfit->second;
      return res;
    }
  }
  else if (res.type == TTM_CONCAT) {
    res.report_error(herr, "Extraneous # ignored");
    goto pp_anyway;
  }
  else if (res.type == TTM_TOSTRING) {
    pp_anyway:
    handle_preprocessor(herr);
    goto top;
  }
  else if (res.type == TT_ENDOFCODE) {
    if (pop_file())
      return token_t(token_basics(TT_ENDOFCODE, filename, line, pos - lpos));
    goto top;
  }
  
  return res;
}

token_t lexer_macro::get_token(error_handler *herr)
{
  top:
  token_t res = lex_base->read_raw(herr);
  if (res.type == TTM_NEWLINE) {
    res.type = TT_ENDOFCODE;
    return res;
  }
  if (res.type == TT_IDENTIFIER) {  // Identifiers and keywords stop here.
    static const char zero[] = "0", one[] = "1";
    // magic number: strlen("defined") == 7; this is unlikely to change
    if (res.content.len == 7 && !strncmp((const char*) res.content.str, "defined", 7))
    {
      lex_base->skip_whitespace();
      bool endpar = lex_base->data[lex_base->pos] == '(';
      if (endpar) {
        ++lex_base->pos;
        lex_base->skip_whitespace();
      }
      
      if (!is_letter(lex_base->data[lex_base->pos]))
        herr->error("Expected identifier to look up as macro", lex_base->filename, lex_base->line, lex_base->pos - lex_base->lpos);
      
      const size_t spos = lex_base->pos;
      while (is_letterd(lex_base->data[++lex_base->pos]));
      string macro(lex_base->data + spos, lex_base->pos - spos);
      
      if (endpar) {
        lex_base->skip_whitespace();
        if (lex_base->data[lex_base->pos] != ')')
          herr->error("Expected ending parenthesis for defined()", lex_base->filename, lex_base->line, lex_base->pos-lex_base->lpos);
        ++lex_base->pos;
      }
      
      return token_t(token_basics(TT_DECLITERAL,lex_base->filename,lex_base->line,lex_base->pos-lex_base->lpos), lex_base->macros.find(macro)==lex_base->macros.end()? zero : one, 1);
    }
    
    const string mn = res.content.toString();
    macro_iter mi = lex_base->macros.find(mn);
    if (mi != lex_base->macros.end()) {
      if (mi->second->argc < 0) {
        bool already_open = false; // Test if we're in this macro already
        quick::stack<openfile>::iterator it = lex_base->files.begin();
        for (unsigned i = 0; i < lex_base->open_macro_count; ++i)
          if (it->filename == mn) { already_open = true; break; }
          else --it;
        if (!already_open) {
          lex_base->enter_macro( mi->second);
          goto top;
        }
      }
      else {
        if (lex_base->parse_macro_function((macro_function*)mi->second, herr))
          goto top;
      }
    }
    
    // This is an undefined identifier; if it were a macro, it would have been expanded by the base lexer.
    // Return token for literal false: zero.
    return token_t(token_basics(TT_DECLITERAL,lex_base->filename,lex_base->line,lex_base->pos-lex_base->lpos), zero, 1);
  }
  else if (res.type == TT_ENDOFCODE) {
    if (lex_base->pop_file())
      return lex_base->read_raw(herr);  // TT_ENDOFCODE
    goto top;
  }
  
  return res;
}

bool lexer_cpp_base::pop_file() {
  if (files.empty())
    return true;
  
  // Close whatever file we have open now
  close();
  
  // Fetch data from top item
  openfile& of = files.top();
  line = of.line, lpos = of.lpos;
  filename = of.filename;
  consume(of.file);
  
  // Pop file stack and return next token in the containing file.
  files.pop();
  
  // Remove any open macro
  if (open_macro_count)
      --open_macro_count;
  
  return false;
}

macro_map lexer_cpp::kludge_map;
lexer_cpp::keyword_map lexer_cpp::keywords;
lexer_cpp_base::lexer_cpp_base(llreader &input, macro_map &pmacros, const char *fname): open_macro_count(0), macros(pmacros), filename(fname), line(1), lpos(0) {
  consume(input); // We are also an llreader. Consume the given one using the inherited method.
}
lexer_cpp::lexer_cpp(llreader &input, macro_map &pmacros, const char *fname): lexer_cpp_base(input, pmacros, fname), mlex(new lexer_macro(this)), mctex(new context_parser(mlex, def_error_handler))
{
  if (keywords.empty()) {
    keywords["asm"] = TT_ASM;
    keywords["__asm"] = TT_ASM;
    keywords["__asm__"] = TT_ASM;
    keywords["class"] = TT_CLASS;
    keywords["decltype"] = TT_DECLTYPE;
    keywords["typeid"] = TT_TYPEID;
    keywords["enum"] = TT_ENUM;
    keywords["extern"] = TT_EXTERN;
    keywords["namespace"] = TT_NAMESPACE;
    keywords["operator"] = TT_OPERATORKW;
    keywords["private"] = TT_PRIVATE;
    keywords["protected"] = TT_PROTECTED;
    keywords["public"] = TT_PUBLIC;
    keywords["friend"] = TT_FRIEND;
    keywords["sizeof"] = TT_SIZEOF;
    keywords["__is_empty"] = TT_ISEMPTY;
    keywords["__is_pod"] = TT_ISEMPTY; // FIXME: yeah, this is a hack
    keywords["struct"] = TT_STRUCT;
    keywords["template"] = TT_TEMPLATE;
    keywords["typedef"] = TT_TYPEDEF;
    keywords["typename"] = TT_TYPENAME;
    keywords["union"] = TT_UNION;
    keywords["using"] = TT_USING;
    keywords["new"] = TT_NEW;
    keywords["delete"] = TT_DELETE;
    
    keywords["const_cast"] = TT_CONST_CAST;
    keywords["static_cast"] = TT_STATIC_CAST;
    keywords["dynamic_cast"] = TT_DYNAMIC_CAST;
    keywords["reinterpret_cast"] = TT_REINTERPRET_CAST;
    
    keywords["auto"] = TT_AUTO;
    keywords["alignas"] = TT_ALIGNAS;
    keywords["alignof"] = TT_ALIGNOF;
    keywords["constexpr"] = TT_CONSTEXPR;
    keywords["noexcept"] = TT_NOEXCEPT;
    keywords["static_assert"] = TT_STATIC_ASSERT;
    
    // GNU Extensions
    keywords["__attribute__"] = TT_INVALID;
    keywords["__extension__"] = TT_INVALID;
    keywords["__typeof__"] = TT_INVALID;
    keywords["__typeof"] = TT_INVALID;
   // keywords["__restrict"] = TT_INVALID;
    
    // MinGW Fuckery
    keywords["__MINGW_IMPORT"] = TT_INVALID;
    
    // C++ Extensions
    keywords["false"] = TT_INVALID;
    keywords["true"] = TT_INVALID;
    
    string x(1,'x');
    kludge_map.clear();
    context::global_macros().swap(kludge_map);
    builtin->add_macro_func("__attribute__", x, string(), false);
    builtin->add_macro_func("__typeof__", x, string("int"), false);
    builtin->add_macro_func("__typeof", x, string("int"), false);
    builtin->add_macro("__extension__", string());
    builtin->add_macro("__MINGW_IMPORT", string());
    builtin->add_macro("false", string(1,'0'));
    builtin->add_macro("true", string(1,'1'));
    context::global_macros().swap(kludge_map);
  }
}
lexer_cpp::~lexer_cpp() {
  delete mlex;
  delete mctex;
}

void lexer_cpp::cleanup() {
  keywords.clear();
  for (macro_iter it = kludge_map.begin(); it != kludge_map.end(); ++it)
    macro_type::free(it->second);
  kludge_map.clear();
}

openfile::openfile() {}
openfile::openfile(const char* fname): filename(fname), line(0), lpos(0) {}
openfile::openfile(const char* fname, string sdir, size_t line_num, size_t line_pos, llreader &consume): filename(fname), searchdir(sdir), line(line_num), lpos(line_pos) { file.consume(consume); }
void openfile::swap(openfile &f) {
  { register const char* tmpl = filename;
  filename = f.filename, f.filename = tmpl; }
  searchdir.swap(f.searchdir);
  register size_t tmpl = line;
  line = f.line, f.line = tmpl;
  tmpl = lpos, lpos = f.lpos, f.lpos = tmpl;
  llreader tmpr;
  tmpr.consume(file);
  file.consume(f.file);
  f.file.consume(tmpr);
}

#undef cfile

lexer_cpp::condition::condition() {}
lexer_cpp::condition::condition(bool t, bool cbt): is_true(t), can_be_true(cbt) {}
