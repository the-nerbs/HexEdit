using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Xml.Linq;

namespace WixHelper
{
    class HeatFilter : Command
    {
        public HeatFilter()
            : base("heat-filter")
        { }


        public override void Execute(XDocument document, CommandLine arguments)
        {
            if (arguments.NamedArguments.TryGetValue("ext", out var extArg))
            {
                ExtensionFilter(extArg.Value!, document);
            }

            if (arguments.NamedArguments.TryGetValue("name", out var nameArg))
            {
                NameFilter(nameArg.Value!, document);
            }
        }

        private static void ExtensionFilter(string exts, XDocument wix)
        {
            var allowedExtensions = new HashSet<string>(exts.Split(';'), StringComparer.OrdinalIgnoreCase);

            foreach (XElement comp in GetComponentsParent(wix).Elements().ToArray())
            {
                if (comp.Element(WixNames.File) is XElement file)
                {
                    string source = file.Attribute(WixNames.Source).Value;
                    string sourceExt = Path.GetExtension(source);

                    if (string.IsNullOrEmpty(sourceExt)
                        || !allowedExtensions.Contains(sourceExt))
                    {
                        RemoveComponent(wix, comp);
                    }
                }
            }
        }

        private static void NameFilter(string filterList, XDocument wix)
        {
            ReadFilterList(filterList, out List<Regex> include, out List<Regex> exclude);

            foreach (XElement comp in GetComponentsParent(wix).Elements().ToArray())
            {
                if (comp.Element(WixNames.File) is XElement file)
                {
                    string source = file.Attribute(WixNames.Source).Value;
                    string sourceName = Path.GetFileName(source);

                    // include if:
                    //  there are no inclusion patterns, or
                    //  any of the inclusion patterns match the filename
                    bool includeComp = include.Count == 0
                                    || include.Any(p => p.IsMatch(sourceName));

                    // exclude if:
                    //  we should not include it, or
                    //  an exclude pattern does match
                    //
                    // note that this gives precedence to the exclusions.
                    if (!includeComp
                        || exclude.Any(p => p.IsMatch(sourceName)))
                    {
                        RemoveComponent(wix, comp);
                    }
                }
            }
        }


        private static void ReadFilterList(string arg, out List<Regex> include, out List<Regex> exclude)
        {
            string[] patterns = arg.Split(';');

            include = new List<Regex>();
            exclude = new List<Regex>();

            foreach (string item in patterns)
            {
                string trimmed = item.Trim();
                string pattern;
                List<Regex> collection;

                if (trimmed.StartsWith("!"))
                {
                    pattern = trimmed.Substring(1).TrimStart();
                    collection = exclude;
                }
                else
                {
                    pattern = trimmed;
                    collection = include;
                }

                collection.Add(new Regex(pattern));
            }
        }

        private static void RemoveComponent(XDocument wix, XElement comp)
        {
            string id = comp.Attribute(WixNames.Id).Value;

            // remove the Component from the Directory/DirectoryRef
            comp.Remove();

            // remove the ComponentRef from the ComponentGroup
            var compGroup = GetComponentRefsParent(wix);
            foreach (var reference in compGroup.Elements().Where(e => e.Attribute(WixNames.Id).Value == id))
            {
                reference.Remove();
            }
        }
    }
}
