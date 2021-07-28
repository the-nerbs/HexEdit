using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WixHelper
{
    class CommandLineArgument
    {
        public int Index { get; }
        public string? Name { get; }
        public string? Value { get; }

        public CommandLineArgument(int index, string? name, string? value)
        {
            Index = index;
            Name = name;
            Value = value;
        }
    }

    class CommandLine
    {
        private readonly CommandLineArgument[] _values;
        private readonly Dictionary<string, CommandLineArgument> _named;
        private readonly string[] _unnamed;


        public IReadOnlyList<CommandLineArgument> Arguments
        {
            get { return _values; }
        }

        public IReadOnlyDictionary<string, CommandLineArgument> NamedArguments
        {
            get { return _named; }
        }

        public IReadOnlyList<string> UnnamedArguments
        {
            get { return _unnamed; }
        }

        public CommandLine(IEnumerable<string> args)
        {
            _values = ParseArgs(args).ToArray();

            _named = _values
                .Where(a => a.Name is not null)
                .ToDictionary(a => a.Name!);

            _unnamed = _values
                .Where(a => a.Name is null)
                .Select(a => a.Value!)
                .ToArray();
        }


        private IEnumerable<CommandLineArgument> ParseArgs(IEnumerable<string> args)
        {
            int argIndex = 0;

            foreach (var arg in args)
            {
                if (IsNamedArg(arg))
                {
                    // named argument, possibly with value
                    SplitArg(arg, out string name, out string? value);
                    yield return new CommandLineArgument(argIndex, name, value);

                    argIndex++;
                }
                else
                {
                    // unnamed argument
                    yield return new CommandLineArgument(argIndex, null, arg);
                    argIndex++;
                }
            }
        }
        
        private static bool IsNamedArg(string arg)
        {
            return arg.Length > 0
                && (arg[0] == '-' || arg[0] == '/');
        }

        private static void SplitArg(string arg, out string name, out string? value)
        {
            Debug.Assert(IsNamedArg(arg));

            int colonIndex = arg.IndexOf(':');

            if (colonIndex < 0)
            {
                name = arg.Substring(1);
                value = null;
            }
            else
            {
                name = arg.Substring(1, colonIndex - 1);
                value = arg.Substring(colonIndex + 1);
            }
        }
    }
}
