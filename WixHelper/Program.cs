using System;
using System.Collections.Generic;
using System.Linq;
using System.Xml.Linq;

namespace WixHelper
{
    class Program
    {
        static Dictionary<string, Command> Commands = new Dictionary<string, Command>()
        {
            ["heat-filter"] = new HeatFilter(),
        };

        static int Main(string[] args)
        {
            try
            {
                if (args.Length < 2)
                {
                    Console.Error.WriteLine($"Usage: <tool> <command> <wix source file> <command arguments>...");
                    return 1;
                }

                Command cmd = Commands[args[0]];
                
                string wixSource = args[1];
                var wix = XDocument.Load(wixSource);

                if (wix.Root.Name != XName.Get("Wix", WixNames.WixNamespace))
                {
                    Console.Error.WriteLine("The selected file is not a WiX source document.");
                    return 1;
                }

                var parsedArgs = new CommandLine(args.Skip(2));

                cmd.Execute(wix, parsedArgs);

                wix.Save(wixSource);

                return 0;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Error while running command: {ex}");
                return 1;
            }
        }
    }
}
