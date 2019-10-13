/**
 * @file lex_cpp.h
 * @brief Header extending the \c lexer base class for parsing C++.
 *
 * This file defines two subclasses of \c jdi::lexer. The first is meant to lex
 * C++ definitions, and so returns a wide range of token types. It will also do
 * any needed preprocessing, handing the second lexer to \c jdi::AST to handle
 * `#if` expression evaluation. The second lexer is much simpler, and treats all
 * identifiers the same.
 *
 * @section License
 *
 * Copyright (C) 2011-2014 Josh Ventura
 * This file is part of JustDefineIt.
 * I only code for
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

#ifndef _LEX_CPP__H
#define _LEX_CPP__H

namespace jdi {
  struct lexer;
  struct lexer_macro;
}

#include <map>
#include <set>
#include <string>
#include <vector>
#include <General/quickstack.h>
#include <General/llreader.h>
#include <System/token.h>
#include <System/macros.h>
#include <API/context.h>

namespace jdi {
  using std::string;
  typedef std::vector<token_t> token_vector;

  struct file_meta {
    string name; ///< The name of the open file or macro.
    string searchdir; ///< The search directory from which this file was included, or the empty string.
    size_t from_line; ///< The index of the line in the file that used or included this one.
    size_t from_lpos; ///< The position in from_line.
  };

  struct OpenFile: file_meta {
    llreader file;
    OpenFile(llreader &&f): file(std::move(f)) {}
  };

  /// Details a macro we entered during preprocessing. Used in error reporting,
  /// and to avoid infinite recursion from unrolling the same macro.
  struct EnteredMacro {
    /// The name of the macro we have entered.
    string name;
    /// The token used in reporting location information.
    token_t origin;
    /// Construct with name and origin token.
    EnteredMacro(string name_, token_t origin_): name(name_), origin(origin_) {}
  };

  /// References or holds a buffer of tokens to emit before processing more.
  /// Brings three principle benefits:
  /// 1. Macros can be entered without re-lexing a buffer.
  /// 2. Our Lexer can be constructed to supply a fixed collection of tokens.
  /// 3. Rewinding is trivial, as previously-read tokens can just be re-stacked.
  struct OpenBuffer {
    /// Tokens in the macro or other buffer (may be owned by someone else!)
    const token_vector &tokens;
    /// If this buffer belongs to a macro, this describes it.
    std::optional<EnteredMacro> macro_info;
    /// Allows us to own the above vector.
    token_vector assembled_token_data;
    /// Scratch space: how far the lexer advanced in this buffer before pushing
    /// a new one and switching to it.
    size_t buf_pos = 0;
    /// Denotes that this buffer was already preprocessed fully.
    bool is_rewind = false;

    OpenBuffer(const OpenBuffer&) = delete;
    OpenBuffer(string macro, token_t origin, const std::vector<token_t> *tokens_):
        tokens(*tokens_), macro_info({macro, origin}) {}
    OpenBuffer(string macro, token_t origin, std::vector<token_t> &&tokens_):
        tokens(assembled_token_data), macro_info({macro, origin}), 
        assembled_token_data(std::move(tokens_)) {}
    OpenBuffer(token_vector &&tokens_):
        tokens(assembled_token_data), macro_info(), 
        assembled_token_data(tokens_) {}

    OpenBuffer(OpenBuffer &&other):
        tokens(&other.tokens == &other.assembled_token_data
               ? assembled_token_data : other.tokens),
        macro_info(std::move(other.macro_info)),
        assembled_token_data(std::move(other.assembled_token_data)),
        buf_pos(other.buf_pos), is_rewind(other.is_rewind) {}
  };

  /**
  The basic C++ lexer. Extracts a single preprocessor token from the given reader.
  Uses @p cfile to include source metadata in the token.
  
  ISO C++ calls for nine phases of translation. The llreader passed to this call
  handles the first (file character mapping), and this routine handles the second
  and third. The data is not physically modified for any of these phases.
  */
  token_t read_token(llreader &cfile, error_handler *herr);

  /// Tokenizes a string with no preprocessing. All words are identifiers.
  /// Will return preprocessing tokens, except for whitespace tokens.
  token_vector tokenize(string fname, string_view str, error_handler *herr);

  /**
  Basic lexing/preprocessing unit; polled by all systems for tokens.
  
  This lexer calls out to `read_token` to handle phases 1-3 of translation.
  It then handles phases four (execution of preprocessing directives), five
  (expansion of string literals), and six (concatenation of adjacent string
  literals). This work needs to be handled when parsing C++11 or greater, as
  string literal contents can be used at compile time to have meaningful
  effect on output (this was also possible in previous versions, but through
  less conventional means that would not normally arise).
  
  Because of that nuance, and the general lookahead-heavy nature of parsing C++,
  token rewinding is a first-class feature of this lexer. When a string literal
  is encountered, another token is read immediately, and is either queued for
  return or concatenated to the current literal (depending on whether it is also
  a string literal). There is also a RAII helper, `look_ahead`, designed to
  facilitate handling of cases such as MVP. This way, a try-like branch of code
  can attempt to evaluate the tree one way, then seamlessly give up and allow
  a later branch to attempt the same.
  
  Thus, this lexer implementation has four layers of token source data:
    1. The open file stack. Files or string buffers (managed by an llreader) are
       lexed for raw tokens.
    2. Macros used within a file are expanded into tokens, and these buffers of
       tokens are stacked. Per ISO, a macro may not appear twice in this stack.
    3. Rewind operations produce queues of tokens. Each queue is stacked.
    4. During normal lexing operations, minor lookahead may be required.
       Tokens read during lookahead are queued at the top of this stack.d
  
  The above stacks are treated in stack order. They are generally populated in
  order from 1-4, but tokens are retrieved in order of 4-1.
  */
  class lexer {
    struct condition;

    llreader cfile;  ///< The current file being read.
    std::vector<OpenFile> files; ///< The files we have open, in the order we entered them.
    std::vector<OpenBuffer> open_buffers; ///< Buffers of tokens to consume.
    error_handler *herr;  ///< Error handler for problems during lex.

    /// Our conditional levels (one for each nested `\#if*`)
    vector<condition> conditionals;

    /// Tokens to return before lexing continues. Generally, these are tokens
    /// that have been expanded from a macro or fetched as lookahead.
    const token_vector *buffered_tokens = nullptr;
    /// The position in the current token buffer.
    size_t buffer_pos;

    /// Buffer to which tokens will be recorded for later re-parse, as needed.
    token_vector *lookahead_buffer = nullptr;

    macro_map &macros; ///< Reference to the \c jdi::macro_map which will be used to store and retrieve macros.

    std::set<string> visited_files; ///< For record and reporting purposes only.
    
    Context *const builtin;

    /// Private base constructor.
    lexer(macro_map &pmacros, error_handler *herr);

    /**
      Utility function designed to handle the preprocessor directive
      pointed to by \c pos upon invoking the function. Note that it should
      be the character directly after the pound pointed to upon invoking
      the function, not the pound itself.
    **/
    void handle_preprocessor();

    /// Tests the given identifier token against currently-defined macros, and
    /// handles expanding it if it is defined and usable in this context.
    bool handle_macro(token_t &identifier);

    /// Converts an identifier token into an appropriate keyword token.
    bool translate_identifier(token_t &identifier);

    /// Function used by the preprocessor to read in macro parameters in compliance with ISO.
    string read_preprocessor_args();
    /// Second-order utility function to skip lines until a preprocessor
    /// directive is encountered, then invoke the handler on the directive it found.
    void skip_to_macro();

    /// Enter a scalar macro, if it has any content.
    /// @param otk  The originating token (naming this macro).
    /// @param ms   The macro scalar to enter.
    void enter_macro(const token_t &otk, const macro_type &ms);
    /// Parse for parameters to a given macro function, if there are any, then evaluate
    /// the macro function and set the open file to reflect the change.
    /// This call should be made while the position is just after the macro name.
    /// @param otk  The originating token (naming this macro function).
    /// @param mf   The macro function to parse
    /// @return Returns whether parameters were encountered and parsed.
    bool parse_macro_function(const token_t &otk, const macro_type &mf);
    /// Parse for parameters to a given macro function, if there are any.
    /// This call should be made while the position is just after the macro
    /// name. That is, the next token should be an opening parenthesis.
    /// If not, this method will return immediately.
    /// @param mf    The macro function to parse.
    /// @param dest  The vector to receive the individual parameters [out].
    /// @return Returns whether parameters were encountered and parsed.
    bool parse_macro_params(const macro_type &mf, vector<vector<token_t>>* dest);
    /// Check if we're currently inside a macro by the given name.
    bool inside_macro(string_view macro_name) const;

    /// Pop the currently open file to return to the file that included it.
    /// @return Returns true if the buffer was successfully popped, and input remains.
    bool pop_file();

    /// Storage mechanism for conditionals, such as `\#if`, `\#ifdef`, and `\#ifndef`.
    struct condition {
      /// True if code in this region is to be parsed
      /// (the condition that was given is true).
      bool is_true;
      /// Indicates that we've seen an else statement already.
      /// If true, this branch must be terminated with an #endif.
      /// This will not be set to true when `elif` or similar are encountered.
      /// In that case, the current condition will be popped, and a new one will
      /// be pushed. In this new condition, `seen_else` will still be false.
      bool seen_else;
      /// Indicates whether the enclosing conditionals were all true.
      /// This prevents if 1 X else if 0 Y else Z from being X and Z,
      /// without having to iterate the entire hierarchy each time.
      bool parents_true;
      /// Convenience constructor.
      condition(bool, bool);
    };

    /// Internal logic to handle preprocessing and fetching a token, as well
    /// as reading tokens off the current buffer, if needed.
    token_t preprocess_and_read_token();
    
   public:
    /** Consumes an llreader and attaches a new \c lex_macro.
        @param input    The file from which to read definitions.
                        This file will be manipulated by the system.
        @param pmacros  A \c jdi::macro_map which will receive
                        (and be probed for) macros.
        @param fname    The name of the file that was first opened.
    **/
    lexer(llreader& input, macro_map &pmacros, error_handler *herr);  // TODO: Have Lexer own pmacros.
    /**
      Consumes a token_vector, returning only the tokens in the vector before
      returning END_OF_CODE.
      @param input    The file from which to read definitions.
                      This file will be manipulated by the system.
      @param fname    The name of the file that was first opened.
    **/
    //lexer(token_vector &&tokens, error_handler *herr);
    /**
      Consumes a token_vector, returning only the tokens in the vector before
      returning END_OF_CODE. Does macro expansion using the macros in the given
      lexer. 
      @param input    The file from which to read definitions.
                      This file will be manipulated by the system.
      @param fname    The name of the file that was first opened.
    **/
    lexer(token_vector &&tokens, const lexer &basis);
    /** Destructor; free the attached macro lexer. **/
    ~lexer();

    /// Read a C++ token, with no scope information.
    token_t get_token();
    /// Read a C++ token, searching the given scope for names.
    token_t get_token_in_scope(definition_scope *scope);
    
    /// RAII type for initiating unbounded lookahead.
    class look_ahead {
      token_vector buffer;
      token_vector *prev_buffer;
      lexer *lex;

     public:
      token_t &push(token_t token) {
        buffer.push_back(token);
        return buffer.back();
      }

      look_ahead(lexer *lex_): prev_buffer(lex_->lookahead_buffer), lex(lex_) {
        lex->lookahead_buffer = &buffer;
      }
      ~look_ahead() {
        if (lex->lookahead_buffer != &buffer) {
          lex->herr->error("LOGIC ERROR: lookahead buffer is not owned");
          abort();
        }
        lex->lookahead_buffer = prev_buffer;
        if (prev_buffer) {
          if (prev_buffer->empty()) {
            prev_buffer->swap(buffer);
          } else {
            prev_buffer->insert(prev_buffer->end(), buffer.begin(), buffer.end());
          }
        }
      }
      void rewind() {
        if (buffer.empty()) return;
        token_vector buf;
        buf.swap(buffer);
        lex->push_rewind_buffer(std::move(buf));
      }
    };

    /// Push a buffer of tokens onto this lexer.
    void push_buffer(OpenBuffer &&buf);

    /// Push a buffer of tokens onto this lexer, and mark them preprocessed.
    void push_rewind_buffer(OpenBuffer &&buf);

    /// Push any construction of an OpenBuffer onto this lexer.
    template<typename... Args> void push_buffer(Args... args) {
      push_buffer(OpenBuffer(std::move(args)...));
    }

    /// Pop the current top buffer.
    void pop_buffer();

    // =========================================================================
    // == Configuration Storage ================================================
    // =========================================================================

    /// Retrieve this lexer's error handler
    error_handler *get_error_handler() { return herr; }
  };
}

#endif
