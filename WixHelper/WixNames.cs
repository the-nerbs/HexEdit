using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;

namespace WixHelper
{
    static class WixNames
    {
        public const string WixNamespace = @"http://schemas.microsoft.com/wix/2006/wi";

        // elements
        public static readonly XName WixRoot = XName.Get("Wix", WixNamespace);
        public static readonly XName Fragment = XName.Get("Fragment", WixNamespace);
        public static readonly XName Directory = XName.Get("Directory", WixNamespace);
        public static readonly XName DirectoryRef = XName.Get("DirectoryRef", WixNamespace);
        public static readonly XName File = XName.Get("File", WixNamespace);
        public static readonly XName Component = XName.Get("Component", WixNamespace);
        public static readonly XName ComponentRef = XName.Get("ComponentRef", WixNamespace);
        public static readonly XName ComponentGroup = XName.Get("ComponentGroup", WixNamespace);

        // attributes
        public static readonly XName Source = XName.Get("Source");
        public static readonly XName Id = XName.Get("Id");
    }
}
