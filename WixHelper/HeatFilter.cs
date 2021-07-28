using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
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
        }

        private void ExtensionFilter(string exts, XDocument wix)
        {
            var allowedExtensions = new HashSet<string>(exts.Split(';'), StringComparer.OrdinalIgnoreCase);

            var compsParent = GetComponentsParent(wix);
            var compGroup = GetComponentRefsParent(wix);

            foreach (XElement comp in compsParent.Elements().ToArray())
            {
                if (comp.Element(WixNames.File) is XElement file)
                {
                    string source = file.Attribute(WixNames.Source).Value;
                    string sourceExt = Path.GetExtension(source);

                    if (string.IsNullOrEmpty(sourceExt)
                        || !allowedExtensions.Contains(sourceExt))
                    {
                        string id = comp.Attribute(WixNames.Id).Value;

                        comp.Remove();

                        foreach (var reference in compGroup.Elements().Where(e => e.Attribute(WixNames.Id).Value == id))
                        {
                            reference.Remove();
                        }
                    }
                }
            }
        }
    }
}
