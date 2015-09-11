/*
 * Android File Transfer for Linux: MTP client for android devices
 * Copyright (C) 2015  Vladimir Menshakov

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef AFT_CLI_COMMAND_H
#define AFT_CLI_COMMAND_H

#include <cli/Tokens.h>
#include <mtp/types.h>
#include <mtp/function_invoker.h>
#include <mtp/make_tuple.h>
#include <functional>
#include <mtp/Demangle.h>

namespace cli
{
	class Session;

	struct Path : public std::string		{ Path(const std::string &path = std::string()): std::string(path) { } };
	struct LocalPath : public std::string	{ LocalPath(const std::string &path = std::string()): std::string(path) { } };

	struct CompletionContext
	{
		cli::Session &				Session;
		size_t						Index;
		std::string					Prefix;
		std::list<std::string> &	Result;
		CompletionContext(cli::Session &s, size_t i, const std::string &p, std::list<std::string> & r):
			Session(s), Index(i), Prefix(p), Result(r) { }
	};

	namespace impl
	{

		template<typename Type>
		struct Completer
		{
			static void Complete(CompletionContext & ctx) { }
		};

		template<>
		struct Completer<Path>
		{
			static void Complete(CompletionContext & ctx);
		};


		template<typename ... Tail>
		struct CompletionForwarder
		{
			static void Complete(CompletionContext & ctx, size_t index)
			{ }
		};

		template<typename First, typename ... Tail>
		struct CompletionForwarder<First, Tail...>
		{
			static void Complete(CompletionContext & ctx, size_t index)
			{
				if (index == 0)
					Completer<typename std::decay<First>::type>::Complete(ctx);
				else
					CompletionForwarder<Tail...>::Complete(ctx, index - 1);
			}
		};
	};

	struct ICommand
	{
		virtual ~ICommand() { }

		virtual void Execute(const Tokens &tokens) const = 0;
		virtual size_t GetArgumentCount() const = 0;
		virtual std::string GetHelpString() const = 0;
		virtual void Complete(CompletionContext &ctx) const = 0;
	};
	DECLARE_PTR(ICommand);

	class BaseCommand : public virtual ICommand
	{
		std::string _help;

	public:
		BaseCommand(const std::string &help): _help(help) { }
		virtual std::string GetHelpString() const
		{ return _help; }
	};

	template<typename ... Args>
	struct Command : public BaseCommand
	{
		typedef std::function<void (Args...)> FuncType;

		FuncType		_func;

		Command(const std::string &help, FuncType && func) : BaseCommand(help), _func(func) { }

		template<typename ...FuncArgs>
		static void Execute(std::function<void (FuncArgs...)> func, const Tokens & tokens)
		{
			auto args = mtp::make_tuple<Tokens::const_iterator, FuncArgs...>(tokens.begin(), tokens.end());
			mtp::invoke(func, args);
		}

		virtual void Execute(const Tokens &tokens) const
		{
			Execute(_func, tokens);
		}

		virtual size_t GetArgumentCount() const
		{ return sizeof...(Args); }

		virtual void Complete(CompletionContext &ctx) const
		{ impl::CompletionForwarder<Args...>::Complete(ctx, ctx.Index); }
	};


}

#endif

