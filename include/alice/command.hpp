/* alice: C++ command shell library
 * Copyright (C) 2017-2018  EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*
  \file command.hpp
  \brief Data structures for command and environment

  \author Mathias Soeken
*/

#pragma once

#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <CLI11.hpp>
#include <any.hpp>
#include <fmt/format.h>
#include <json.hpp>

#include "detail/logging.hpp"
#include "detail/utils.hpp"
#include "store.hpp"
#include "store_api.hpp"

namespace alice
{

class command;

template<class... S>
class cli;

/*! \brief Shell environment

  The environment gives access to shell related properties, e.g., the commands
  and stores.
*/
class environment
{
public:
  /*! \brief Smart pointer alias for environment */
  using ptr = std::shared_ptr<environment>;

  /*! \brief Retrieves store from environment

    The store can be accessed using its type.
  */
  template<typename T>
  store_container<T>& store() const
  {
    constexpr auto key = store_info<T>::key;

    return *( reinterpret_cast<alice::store_container<T>*>( _stores.at( key ).get() ) );
  }

  /*! \brief Checks whether environment has store for some data type
  
    Stores are defined by their type.
  */
  template<typename T>
  bool has_store() const
  {
    constexpr auto key = store_info<T>::key;

    return _stores.find( key ) != _stores.end();
  }

  /*! \brief Retreives standard output stream

    This method returns a reference to the current standard output stream.  In
    stand-alone application mode, this is ``std::cout`` by default, but can be
    changed.  Users should aim for not printing to ``std::cout`` directly in a
    command, but use ``env->out()`` instead.
  */
  inline std::ostream& out() const { return *_out; }

  /*! \brief Retreives standard error stream

    This method returns a reference to the current standard error stream.  In
    stand-alone application mode, this is ``std::cerr`` by default, but can be
    changed.  Users should aim for not printing to ``std::cerr`` directly in a
    command, but use ``env->err()`` instead.
  */
  inline std::ostream& err() const { return *_err; }

  /*! \brief Changes output and error streams

    This method allows to change the output streams which are returned by
    ``out()`` and ``err()``.
  */
  inline void reroute( std::ostream& new_out, std::ostream& new_err )
  {
    _out = &new_out;
    _err = &new_err;
  }

  /*! \brief Returns map of commands
  
    The keys correspond to the command names in the shell.
  */
  inline const std::unordered_map<std::string, std::shared_ptr<command>>& commands() const
  {
    return _commands;
  }

  /*! \brief Returns map of categories
  
    Keys are catgory names pointing to a vector of command names that can be
    used to index into ``commands()``.
  */
  inline const std::unordered_map<std::string, std::vector<std::string>>& categories() const
  {
    return _categories;
  }

  /*! \brief Returns a map of aliases

    Keys are the alias regular expressions mapping to substitutions.
  */
  inline const std::unordered_map<std::string, std::string>& aliases() const
  {
    return _aliases;
  }

  /*! \brief Get environment variable

    Finds an environment variable or returns a default value. Variables can be
    set with the ``set`` command.

    \param key Key for the value
    \param default_value Default value
  */
  inline const std::string& variable( const std::string& key, const std::string& default_value = std::string() ) const
  {
    const auto it = _variables.find( key );
    return it != _variables.end() ? it->second : default_value;
  }

private:
  /*! \brief Adds store to environment */
  template<typename T>
  void add_store()
  {
    constexpr auto key = store_info<T>::key;
    constexpr auto name = store_info<T>::name;

    _stores.emplace( key, std::shared_ptr<void>( new alice::store_container<T>( name ) ) );
  }

private:
  /* cli is friend to add stores, categories, and commands */
  template<class... S>
  friend class cli;

  /* alias command is friend to insert aliases */
  friend class alias_command;

  /* help command is friend to sort categories */
  friend class help_command;

  /* set command is friend to access variables */
  friend class set_command;

  /* quit command is friend to update quit flag */
  friend class quit_command;

private:
  std::unordered_map<std::string, std::shared_ptr<void>> _stores;
  std::unordered_map<std::string, std::shared_ptr<command>> _commands;
  std::unordered_map<std::string, std::vector<std::string>> _categories;
  std::unordered_map<std::string, std::string> _aliases;
  std::unordered_map<std::string, std::string> _variables;

  bool log{false};
  alice::detail::logger logger;
  bool quit{false};

  std::ostream* _out = &std::cout;
  std::ostream* _err = &std::cerr;
};

/*! \brief Command base class */
class command
{
public:
  using rule_t = std::pair<std::function<bool()>, std::string>;
  using rules_t = std::vector<rule_t>;

  /*! \brief Default constructor

    The shell environment that is passed as the first argument should be the one
    from the ``alice::cli`` instance.  Typically, commands are constructed and
    added using the macro API, e.g., ``ALICE_COMMAND`` or ``ALICE_ADD_COMMAND``.

    \param env Shell environment
    \param caption Short (one-line) description of the command
  */
  command( const environment::ptr& env, const std::string& caption )
      : env( env ),
        opts( caption ),
        scaption( caption )
  {
  }

protected:
  /*! \brief Returns rules to check validity of command line arguments */
  virtual rules_t validity_rules() const { return {}; }

  /*! \brief Executes the command */
  virtual void execute() = 0;

/* A small hack to get the Python bindings to work */
#if defined ALICE_PYTHON
public:
#endif
  /*! \brief Returns logging data */
  virtual nlohmann::json log() const { return nullptr; }

public:
  /*! \brief Returns command short description */
  inline const auto& caption() const { return scaption; }

  /*! \brief Adds a flag to the command

    This function should be called in the constructor when the program options
    are set up.  See https://github.com/CLIUtils/CLI11#adding-options for more
    information.

    This is a shortcut to ``opts.add_flag``.

    \param name Flag names (short flags are prefixed with a single dash, long
                flags with a double dash), multiple flag names are separated by
                a comma.
    \param description Description for the help text
    \return Option instance
  */
  inline auto add_flag( const std::string& name, const std::string& description )
  {
    return opts.add_flag( name, description );
  }

  /*! \brief Adds an option to the command

    This function should be called in the constructor when the program options
    are set up.  See https://github.com/CLIUtils/CLI11#adding-options for more
    information.

    This is a shortcut to ``opts.add_option``.

    \param name Option names (short options are prefixed with a single dash,
                long options with a double dash, positional options without any
                dash), multiple option names are separated by a comma.
    \param value Reference where option value is store
    \param description Description for the help text
    \param defaulted Use initial value to ``value`` as default value
    \return Option instance
  */
  template<typename T>
  inline auto add_option( const std::string& name, T& value, const std::string& description, bool defaulted = false )
  {
    return opts.add_option( name, value, description, defaulted );
  }

  /*! \brief Adds an anonymous option to the command

    Unlike the other method, this method adds an option, but takes the value
    reference from the command itself.  This is especially helpful when using
    the store API, e.g., ``can_read`` together with ``read``, where command line
    options are setup in one function but used in another.

    Use a type as template argument to specify the type of the option value and
    use ``option_value`` to return the option value using any of the option
    names (incl. possible dashes).

    \param name Option names (short options are prefixed with a single dash,
                long options with a double dash, positional options without any
                dash), multiple option names are separated by a comma.
    \param description Description for the help text
    \return Option instance
  */
  template<typename T = std::string>
  inline auto add_option( const std::string& name, const std::string& description )
  {
    const auto index = options.size();
    options.push_back( T() );
    auto opt = opts.add_option( name, options.back(), description );

    for ( const auto& name : detail::split( opt->get_name(), "," ) )
    {
      option_index[name] = index;
    }

    return opt;
  }

  /*! \brief Returns the value for an anonymous option

    Use any of the option names to access a value.  For example, if the option
    names were ``"--option,-o"``, then one can use both ``"--option"`` and
    ``"-o"`` as value for the ``name`` parameter.  It is important that the same
    type is used to retrieve the option value that was used to add the anonymous
    option.  If the option name does not point to an anonymous option, a default
    value is returned.

    \param name One of the option names that was used to create the option
    \param default_value Dafault value, if name does not point to anonymous option
    \return Option value
  */
  template<typename T = std::string>
  inline T option_value( const std::string& name, const T& default_value = T() ) const
  {
    const auto it = option_index.find( name );
    if ( it == option_index.end() )
    {
      return default_value;
    }
    else
    {
      const linb::any& a = options.at( it->second );
      return *linb::any_cast<T>( &a );
    }
  }

  /*! \brief Checks whether an option was set when calling the command

    Any of the option names can be passed, with our without dashes.

    \param name Option name
  */
  inline bool is_set( const std::string& name ) const
  {
    if ( name.empty() )
    {
      return false;
    }
    else if ( name.front() == '-' )
    {
      return opts.count( name );
    }
    else if ( name.size() == '1' )
    {
      return opts.count( "-" + name );
    }
    else
    {
      return opts.count( "--" + name );
    }
  }

  /*! \brief Returns a store

    Short cut for ``env->store<T>()``.
  */
  template<typename T>
  inline store_container<T>& store() const
  {
    return env->store<T>();
  }

/* A small hack to get the Python bindings to work */
#if defined ALICE_PYTHON
public:
#else
protected:
#endif
  /*! \cond PRIVATE */
  virtual bool run( const std::vector<std::string>& args )
  {
    opts.reset();

    /* copy arguments (seems important to get the right grouping of arguments) */
    std::vector<std::string> _args( args.size() - 1 );
    std::transform( args.rbegin(), args.rend() - 1, _args.begin(), []( const auto& s ) {
      if ( s.size() > 2 && s.front() == '"' && s.back() == '"' )
      {
        return s.substr( 1, s.size() - 2 );
      }

      const auto c_eq = s.find( '=' );
      const auto c_q = s.find( '"' );

      if ( c_eq != std::string::npos&& c_q != std::string::npos&& c_q == c_eq + 1 && s.back() == '"' )
      {
        return s.substr( 0, c_eq + 1 ) + s.substr( c_q + 1, s.size() - c_q - 2 );
      }

      return s;
    } );

    try
    {
      opts.parse( _args );
    }
    catch ( const CLI::CallForHelp& e )
    {
      env->out() << opts.help();
      return false;
    }
    catch ( const CLI::ParseError& e )
    {
      env->err() << "[e] " << e.what() << std::endl;
      return false;
    }

    for ( const auto& p : validity_rules() )
    {
      if ( !p.first() )
      {
        env->err() << "[e] " << p.second << std::endl;
        return false;
      }
    }

    execute();
    return true;
  }
  /*! \endcond */

public:
  environment::ptr env;
  CLI::App opts;

private:
  std::string scaption;
  std::vector<linb::any> options;
  std::unordered_map<std::string, unsigned> option_index;

private:
  template<typename... S>
  friend class cli;

  template<typename StoreType, typename Tag>
  friend bool can_read( command& cmd );

  template<typename StoreType, typename Tag>
  friend StoreType read( const std::string& filename, command& cmd );

  template<typename StoreType, typename Tag>
  friend bool can_write( command& cmd );

  template<typename StoreType, typename Tag>
  friend void write( const StoreType& element, const std::string& filename, command& cmd );
};

template<typename S>
int add_option_helper( CLI::App& opts )
{
  constexpr auto option = store_info<S>::option;
  constexpr auto mnemonic = store_info<S>::mnemonic;
  constexpr auto name_plural = store_info<S>::name_plural;

  if ( strlen( mnemonic ) == 1u )
  {
    opts.add_flag( fmt::format( "-{},--{}", mnemonic, option ), name_plural );
  }
  else
  {
    opts.add_flag( fmt::format( "--{}", option ), name_plural );
  }
  return 0;
}

template<typename T>
bool any_true_helper( std::initializer_list<T> list )
{
  for ( auto i : list )
  {
    if ( i )
    {
      return true;
    }
  }

  return false;
}

template<typename T>
bool exactly_one_true_helper( std::initializer_list<T> list )
{
  auto current = false;

  for ( auto i : list )
  {
    if ( i )
    {
      if ( current )
      {
        return false;
      }
      current = true;
    }
  }

  return current;
}
}
