using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;
using System.Xml.XPath;

namespace WixHelper
{
    abstract class Command
    {
        public string Name { get; }


        public Command(string name)
        {
            Name = name;
        }


        public abstract void Execute(XDocument document, CommandLine arguments);


        protected static XElement GetComponentsParent(XDocument wix)
        {
            // HarvestDirectory will yield a document like:
            // Wix
            //  Fragment
            //    DirectoryRef
            //      [Directory]
            //        Component...
            //  Fragment
            //    ComponentGroup
            //      ComponentRef
            //
            // the bracketed Directory element only exists if
            // %(HarvestDirectory.SuppressRootDirectory) is not true.
            XElement parent = wix
                .Element(WixNames.WixRoot)
                .Element(WixNames.Fragment)
                .Element(WixNames.DirectoryRef);

            if (parent.Elements().Count() == 1
                && parent.Element(WixNames.Directory) is XElement dir)
            {
                parent = dir;
            }

            return parent;
        }

        protected static XElement GetComponentRefsParent(XDocument wix)
        {
            return wix
                .Element(WixNames.WixRoot)
                .Elements(WixNames.Fragment).Skip(1).First()
                .Element(WixNames.ComponentGroup);
        }
    }
}
