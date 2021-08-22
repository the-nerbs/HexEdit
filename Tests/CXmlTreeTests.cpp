#include "Stdafx.h"
#include "utils/File.h"
#include "utils/Garbage.h"
#include "utils/TestFiles.h"

#include "Xmltree.h"

#include <catch.hpp>

#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <system_error>


static constexpr const char* TestFile = R"(TestFiles/CXmlTreeTestFile.xml)";


TEST_CASE("CXmlTree constructor tests")
{
    SECTION("construct without file")
    {
        CXmlTree x;

        CHECK(x.m_pdoc);

        CHECK(x.GetFileName() == "");

        x.SetFileName("testing");
        CHECK(x.GetFileName() == "testing");
        x.SetFileName("");
        CHECK(x.GetFileName() == "");

        CHECK(x.GetDTDName() == "");
        CHECK(x.Error() == false);
        CHECK(x.ErrorMessage() == "");
        CHECK(x.ErrorLine() == 0);
        CHECK(x.ErrorLineText() == "");
        CHECK(x.IsModified() == false);

        x.SetModified(true);
        CHECK(x.IsModified() == true);
        x.SetModified(false);
        CHECK(x.IsModified() == false);

        CHECK(x.DumpXML() == "");
    }

    SECTION("construct with file")
    {
        CXmlTree x{ TestFile };

        CHECK(x.m_pdoc);
        CHECK(x.GetFileName() == TestFile);
        CHECK(x.GetDTDName() == "Root");
        CHECK(x.Error() == false);
        CHECK(x.ErrorMessage() == "");
        CHECK(x.ErrorLine() == 0);
        CHECK(x.ErrorLineText() == "");
        CHECK(x.IsModified() == false);
        CHECK(x.DumpXML() != "");
    }
}

TEST_CASE("CXmlTree loading")
{
    CXmlTree x;

    SECTION("load from file")
    {
        REQUIRE(x.LoadFile(TestFile));

        CHECK(x.GetFileName() == TestFile);
        CHECK(x.GetDTDName() == "Root");
        CHECK(x.Error() == false);
        CHECK(x.ErrorMessage() == "");
        CHECK(x.ErrorLine() == 0);
        CHECK(x.ErrorLineText() == "");
        CHECK(x.IsModified() == false);
    }

    SECTION("load from string")
    {
        CString str = R"(<?xml version="1.0" encoding="utf-8"?>
<Root>
  <TestElem1 test-attr-1="a" test-attr-2="b">
    <TestElem1Child1>text</TestElem1Child1>
    <TestElem1Child2>1</TestElem1Child2>
  </TestElem1>
  <TestElem2>
    <TestElem2Child1 />
  </TestElem2>
</Root>)";

        bool loaded = x.LoadStringA(str);

        if (!loaded)
        {
            CString str = x.ErrorMessage();
            FAIL(str);
        }

        CHECK(x.GetFileName() == "");
        CHECK(x.GetDTDName() == "");
        CHECK(x.Error() == false);
        CHECK(x.ErrorMessage() == "");
        CHECK(x.ErrorLine() == 0);
        CHECK(x.ErrorLineText() == "");
        CHECK(x.IsModified() == true);

        // note: MSXML:
        //  - uses tabs,
        //  - removes the encoding decl,
        //  - does not put a space in between an element name and the slash on empty elements (eg: `<a/>`)
        //  - puts a line ending on the last line.
        CString expected =
            "<?xml version=\"1.0\"?>\r\n"
            "<Root>\r\n"
            "\t<TestElem1 test-attr-1=\"a\" test-attr-2=\"b\">\r\n"
            "\t\t<TestElem1Child1>text</TestElem1Child1>\r\n"
            "\t\t<TestElem1Child2>1</TestElem1Child2>\r\n"
            "\t</TestElem1>\r\n"
            "\t<TestElem2>\r\n"
            "\t\t<TestElem2Child1/>\r\n"
            "\t</TestElem2>\r\n"
            "</Root>\r\n";
        CString xml = x.DumpXML();
        CHECK(xml == expected);
    }

    SECTION("load from string - not xml")
    {
        CString source = "{ \"surprise\": \"its actually me, json\" }";
        bool success = x.LoadStringA(source);

        CHECK(success == false);

        CHECK(x.Error() == true);

        // error is on a specific line
        CHECK(x.ErrorLine() == 1);
        CHECK(x.ErrorLineText() == source);

        // the error message is just passed up from MSXML, so it's
        // not really valid to assert the message content here.
        CHECK(x.ErrorMessage() != "");
    }

    SECTION("load from string - incomplete xml")
    {
        CString source = R"(<?xml version="1.0" encoding="utf-8"?>
<Root>
  <TestElem1 test-attr-1="a" test-attr-2="b">
    <TestElem1Child1>text</TestElem1Child1>
    <TestElem1Child2>1</TestElem1Child2>
  </TestElem1>
  <TestElem2>
    <TestElem2Child1 />)";
        // missing: </TestElem2> </Root>)";

        bool success = x.LoadStringA(source);

        CHECK(success == false);

        CHECK(x.Error() == true);

        // error in document, not specific to a line.
        CHECK(x.ErrorLine() == 0);
        CHECK(x.ErrorLineText() == "");

        // the error message is just passed up from MSXML, so it's
        // not really valid to assert the message content here.
        CHECK(x.ErrorMessage() != "");
    }
}

TEST_CASE("CXmlTree saving")
{
    CXmlTree x;
    x.LoadStringA(R"(<?xml version="1.0" encoding="utf-8"?>
<Root>
  <TestElem1 test-attr-1="a" test-attr-2="b">
    <TestElem1Child1>text</TestElem1Child1>
    <TestElem1Child2>1</TestElem1Child2>
  </TestElem1>
  <TestElem2>
    <TestElem2Child1 />
  </TestElem2>
</Root>)");

    SECTION("save to path")
    {
        x.SetFileName("TEST_PATH");

        CString path = TestFiles::GetMutableFilePath();
        REQUIRE(x.Save(path));

        // Save *does not* change the set file name
        CHECK(x.GetFileName() == "TEST_PATH");

        // note: on save, MSXML:
        //  - uses tabs,
        //  - *includes* the encoding decl,
        //  - does not put a space in between an element name and the slash on empty elements (eg: `<a/>`)
        //  - puts a line ending on the last line.
        CString expected =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
            "<Root>\r\n"
            "\t<TestElem1 test-attr-1=\"a\" test-attr-2=\"b\">\r\n"
            "\t\t<TestElem1Child1>text</TestElem1Child1>\r\n"
            "\t\t<TestElem1Child2>1</TestElem1Child2>\r\n"
            "\t</TestElem1>\r\n"
            "\t<TestElem2>\r\n"
            "\t\t<TestElem2Child1/>\r\n"
            "\t</TestElem2>\r\n"
            "</Root>\r\n";

        CString actual = File::ReadAllText(path);

        CHECK(expected == actual);
    }

    SECTION("save to set file name")
    {
        CString path = TestFiles::GetMutableFilePath();
        x.SetFileName(path);

        REQUIRE(x.Save());

        // note: on save, MSXML:
        //  - uses tabs,
        //  - *includes* the encoding decl,
        //  - does not put a space in between an element name and the slash on empty elements (eg: `<a/>`)
        //  - puts a line ending on the last line.
        CString expected =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
            "<Root>\r\n"
            "\t<TestElem1 test-attr-1=\"a\" test-attr-2=\"b\">\r\n"
            "\t\t<TestElem1Child1>text</TestElem1Child1>\r\n"
            "\t\t<TestElem1Child2>1</TestElem1Child2>\r\n"
            "\t</TestElem1>\r\n"
            "\t<TestElem2>\r\n"
            "\t\t<TestElem2Child1/>\r\n"
            "\t</TestElem2>\r\n"
            "</Root>\r\n";

        CString actual = File::ReadAllText(path);

        CHECK(expected == actual);
    }

    SECTION("save to set file name with no file name set fails")
    {
        CString path = TestFiles::GetMutableFilePath();
        REQUIRE(x.Save() == false);
    }

    SECTION("save to read-only file fails")
    {
        CString path = TestFiles::GetMutableFilePath();

        file_attrs attrs = File::GetAttributes(path);
        File::SetAttributes(path, attrs | file_attrs::readonly);

        REQUIRE(x.Save(path) == false);
    }
}


TEST_CASE("CXmlTree::CElt constructor tests")
{
    CXmlTree doc;
    doc.LoadStringA(R"(<?xml version="1.0" encoding="utf-8"?>
<Root>
  <TestElem1 test-attr-1="a" test-attr-2="b">
    <TestElem1Child1>text</TestElem1Child1>
    <TestElem1Child2>1</TestElem1Child2>
  </TestElem1>
  <TestElem2>
    <TestElem2Child1 />
  </TestElem2>
</Root>)");


    SECTION("CXmlTree::CElt::CElt()")
    {
        CXmlTree::CElt elem{};

        CHECK(elem.m_pelt == nullptr);
        CHECK(elem.GetOwner() == nullptr);

        CHECK(elem.GetName() == "");
        CHECK(elem.IsEmpty());

        // everything else ASSERTs that the CElt is valid.
    }


    SECTION("CXmlTree::CElt::CElt(IXMLDOMNodePtr, CXmlTree*) - root element")
    {
        MSXML2::IXMLDOMElementPtr root = doc.m_pdoc->documentElement;

        CXmlTree::CElt elem{ root, &doc };

        CHECK(elem.m_pelt == root);
        CHECK(elem.GetOwner() == &doc);

        CHECK(elem.GetName() == "Root");
        CHECK(elem.IsEmpty() == false);
        CHECK(elem.GetNumChildren() == 2);
    }

    SECTION("CXmlTree::CElt::CElt(IXMLDOMNodePtr, CXmlTree*) - non-root element")
    {
        MSXML2::IXMLDOMElementPtr root = doc.m_pdoc->documentElement;
        MSXML2::IXMLDOMElementPtr firstChild = root->GetfirstChild();

        CXmlTree::CElt elem{ firstChild, &doc };

        CHECK(elem.m_pelt == firstChild);
        CHECK(elem.GetOwner() == &doc);

        CHECK(elem.GetName() == "TestElem1");
        CHECK(elem.IsEmpty() == false);
        CHECK(elem.GetNumChildren() == 2);
    }


    SECTION("CXmlTree::CElt::CElt(LPCTSTR, CXmlTree*)")
    {
        CXmlTree::CElt elem{ "Child", &doc };

        CHECK(elem.m_pelt != nullptr);
        CHECK(elem.GetOwner() == &doc);

        CHECK(elem.GetName() == "Child");
        CHECK(elem.IsEmpty() == false);
        CHECK(elem.GetNumChildren() == 0);
    }
}

TEST_CASE("CXmlTree::CElt - navigation")
{
    CXmlTree doc;
    doc.LoadStringA(R"(<?xml version="1.0" encoding="utf-8"?>
<Root>
  <TestElem1>
    <TestElem1Child1>text</TestElem1Child1>
    <TestElem1Child2>1</TestElem1Child2>
  </TestElem1>
  <TestElem2>
    <TestElem2Child1 />
  </TestElem2>
</Root>)");

    CXmlTree::CElt root = doc.GetRoot();


    SECTION("GetParent - root element")
    {
        CXmlTree::CElt parent = root.GetParent();

        CHECK(parent.IsEmpty());
    }

    SECTION("GetParent - non-root element")
    {
        CXmlTree::CElt parent = root.GetChild(1).GetParent();

        CHECK(parent.GetName() == "Root");
        CHECK(parent.m_pelt == root.m_pelt);
    }


    SECTION("GetFirstChild")
    {
        CXmlTree::CElt child = root.GetFirstChild();

        CHECK(child.GetName() == "TestElem1");
    }


    SECTION("GetChild - by index")
    {
        CXmlTree::CElt child = root.GetChild(1);

        CHECK(child.GetName() == "TestElem2");
    }

    SECTION("GetChild - by index - nonexistent")
    {
        CXmlTree::CElt child = root.GetChild(2);

        CHECK(child.IsEmpty());
    }


    SECTION("GetChild - by name")
    {
        CXmlTree::CElt child = root.GetChild("TestElem2");

        CHECK(child.GetName() == "TestElem2");
    }

    SECTION("GetChild - by name - nonexistent")
    {
        CXmlTree::CElt child = root.GetChild("TestElemDoesNotExist");

        CHECK(child.IsEmpty());
    }


    SECTION("next sibling - pre-increment")
    {
        CXmlTree::CElt elem = root.GetFirstChild();

        CHECK(elem.GetName() == "TestElem1");

        CXmlTree::CElt result = ++elem;

        CHECK(elem.GetName() == "TestElem2");
        CHECK(result.GetName() == "TestElem2");
    }

    SECTION("next sibling - pre-increment past the end")
    {
        CXmlTree::CElt elem = root.GetChild(1);

        CHECK(elem.GetName() == "TestElem2");

        CXmlTree::CElt result = ++elem;

        CHECK(elem.IsEmpty());
        CHECK(result.IsEmpty());
    }

    SECTION("next sibling - post-increment")
    {
        CXmlTree::CElt elem = root.GetFirstChild();

        CHECK(elem.GetName() == "TestElem1");

        CXmlTree::CElt result = elem++;

        CHECK(elem.GetName() == "TestElem2");
        CHECK(result.GetName() == "TestElem1");
    }

    SECTION("next sibling - post-increment past the end")
    {
        CXmlTree::CElt elem = root.GetChild(1);

        CHECK(elem.GetName() == "TestElem2");

        CXmlTree::CElt result = elem++;

        CHECK(elem.IsEmpty());
        CHECK(result.GetName() == "TestElem2");
    }


    SECTION("previous sibling - pre-decrement")
    {
        CXmlTree::CElt elem = root.GetChild("TestElem2");

        CHECK(elem.GetName() == "TestElem2");

        CXmlTree::CElt result = --elem;

        CHECK(elem.GetName() == "TestElem1");
        CHECK(result.GetName() == "TestElem1");
    }

    SECTION("previous sibling - pre-decrement past start")
    {
        CXmlTree::CElt elem = root.GetFirstChild();

        CHECK(elem.GetName() == "TestElem1");

        CXmlTree::CElt result = --elem;

        CHECK(elem.IsEmpty());
        CHECK(result.IsEmpty());
    }

    SECTION("previous sibling - post-decrement")
    {
        CXmlTree::CElt elem = root.GetChild("TestElem2");

        CHECK(elem.GetName() == "TestElem2");

        CXmlTree::CElt result = elem--;

        CHECK(elem.GetName() == "TestElem1");
        CHECK(result.GetName() == "TestElem2");
    }

    SECTION("previous sibling - post-decrement past start")
    {
        CXmlTree::CElt elem = root.GetFirstChild();

        CHECK(elem.GetName() == "TestElem1");

        CXmlTree::CElt result = elem--;

        CHECK(elem.IsEmpty());
        CHECK(result.GetName() == "TestElem1");
    }
}

TEST_CASE("CXmlTree::CElt - attributes")
{
    CXmlTree doc;
    doc.LoadStringA(R"(<?xml version="1.0" encoding="utf-8"?>
<Root test-attr="a" test-attr-escapes="&lt;&gt;&amp;&quot;&apos;" />)");

    CXmlTree::CElt root = doc.GetRoot();
    doc.SetModified(false);


    SECTION("GetAttr")
    {
        CString value = root.GetAttr("test-attr");
        CHECK(value == "a");
    }

    SECTION("GetAttr - escape sequences")
    {
        CString value = root.GetAttr("test-attr-escapes");
        CHECK(value == "<>&\"'");
    }

    SECTION("GetAttr - nonexistent")
    {
        CString value = root.GetAttr("test-attr-does-not-exist");
        CHECK(value == "");
    }


    SECTION("SetAttr")
    {
        root.SetAttr("test-attr", "b");

        CString value = root.GetAttr("test-attr");
        CHECK(value == "b");
        CHECK(doc.IsModified());
    }

    SECTION("SetAttr - new attribute")
    {
        root.SetAttr("test-new-attr", "some value");

        CString value = root.GetAttr("test-new-attr");
        CHECK(value == "some value");
        CHECK(doc.IsModified());
    }

    SECTION("SetAttr - escape sequences")
    {
        root.SetAttr("test-new-attr", "<>&\"'");

        CString value = root.GetAttr("test-new-attr");
        CHECK(value == "<>&\"'");
        CHECK(doc.IsModified());
    }


    SECTION("RemoveAttr")
    {
        root.RemoveAttr("test-attr");

        CString value = root.GetAttr("test-attr");
        CHECK(value == "");
        CHECK(doc.IsModified());
    }

    SECTION("RemoveAttr - nonexistent")
    {
        root.RemoveAttr("test-attr-does-not-exist");

        CString value = root.GetAttr("test-attr-does-not-exist");
        CHECK(value == "");
        CHECK(doc.IsModified() == false);
    }
}

TEST_CASE("CXmlTree::CElt - mutations")
{
    CXmlTree doc;
    doc.LoadStringA(R"(
<Root>
  <Child />
</Root>)");

    CXmlTree::CElt root = doc.GetRoot();
    CXmlTree::CElt child = root.GetFirstChild();

    doc.SetModified(false);

    SECTION("InsertNewChild")
    {
        CXmlTree::CElt elem = root.InsertNewChild("TestChild");
        CHECK(doc.IsModified());

        CHECK(elem.GetOwner() == &doc);
        CHECK(root.GetNumChildren() == 2);

        CXmlTree::CElt c = root.GetFirstChild();
        CHECK(c.GetName() == "Child");

        c++;
        CHECK(c.GetName() == "TestChild");
    }

    SECTION("InsertNewChild - before existing")
    {
        CXmlTree::CElt existing = root.GetFirstChild();
        CXmlTree::CElt elem = root.InsertNewChild("TestChild", &existing);
        CHECK(doc.IsModified());

        CHECK(elem.GetOwner() == &doc);
        CHECK(root.GetNumChildren() == 2);

        CXmlTree::CElt c = root.GetFirstChild();
        CHECK(c.GetName() == "TestChild");

        c++;
        CHECK(c.GetName() == "Child");
    }


    SECTION("InsertChild")
    {
        CXmlTree::CElt elem{ "TestChild", &doc };

        CXmlTree::CElt result = root.InsertChild(elem);
        CHECK(doc.IsModified());
        CHECK(result.m_pelt == elem.m_pelt);
        CHECK(result.GetOwner() == &doc);

        CXmlTree::CElt c = root.GetFirstChild();
        CHECK(c.GetName() == "Child");

        c++;
        CHECK(c.GetName() == "TestChild");
    }

    SECTION("InsertChild - before existing")
    {
        CXmlTree::CElt elem{ "TestChild", &doc };

        CXmlTree::CElt result = root.InsertChild(elem, &child);
        CHECK(doc.IsModified());
        CHECK(result.m_pelt == elem.m_pelt);
        CHECK(result.GetOwner() == &doc);

        CXmlTree::CElt c = root.GetFirstChild();
        CHECK(c.GetName() == "TestChild");

        c++;
        CHECK(c.GetName() == "Child");
    }


    SECTION("InsertClone")
    {
        CXmlTree::CElt elem{ "TestChild", &doc };

        CXmlTree::CElt result = root.InsertClone(elem);
        CHECK(doc.IsModified());
        CHECK(result.m_pelt != elem.m_pelt);
        CHECK(result.GetOwner() == &doc);

        CXmlTree::CElt c = root.GetFirstChild();
        CHECK(c.GetName() == "Child");

        c++;
        CHECK(c.GetName() == "TestChild");
    }

    SECTION("InsertClone - before existing")
    {
        CXmlTree::CElt elem{ "TestChild", &doc };

        CXmlTree::CElt result = root.InsertClone(elem, &child);
        CHECK(doc.IsModified());
        CHECK(result.m_pelt != elem.m_pelt);
        CHECK(result.GetOwner() == &doc);

        CXmlTree::CElt c = root.GetFirstChild();
        CHECK(c.GetName() == "TestChild");

        c++;
        CHECK(c.GetName() == "Child");
    }


    SECTION("ReplaceChild")
    {
        CXmlTree::CElt elem{ "TestChild", &doc };

        root.ReplaceChild(elem, child);

        CHECK(doc.IsModified());

        CXmlTree::CElt c = root.GetFirstChild();
        CHECK(c.GetName() == "TestChild");
    }


    SECTION("DeleteChild")
    {
        root.InsertNewChild("Child2");

        CXmlTree::CElt removed = root.DeleteChild(child);

        CHECK(doc.IsModified());

        CHECK(removed.GetName() == "Child");
        CHECK(removed.GetOwner() == &doc);

        CHECK(root.GetNumChildren() == 1);
        CHECK(root.GetFirstChild().GetName() == "Child2");
    }


    SECTION("DeleteAllChildren")
    {
        root.InsertNewChild("Child2");
        root.InsertNewChild("Child3");

        root.DeleteAllChildren();

        CHECK(doc.IsModified());
        CHECK(root.GetNumChildren() == 0);
    }
}

TEST_CASE("CXmlTree::CElt - Clone")
{
    CXmlTree doc;
    doc.LoadStringA(R"(
<Root attr-1="a">
  <Child attr-2="b" />
</Root>)");

    CXmlTree::CElt root = doc.GetRoot();
    CXmlTree::CElt child = root.GetFirstChild();

    doc.SetModified(false);


    SECTION("Clone default")
    {
        CXmlTree::CElt elem{};

        CXmlTree::CElt result = elem.Clone();

        CHECK(result.m_pelt == nullptr);
        CHECK(result.GetOwner() == nullptr);
    }

    SECTION("Clone root")
    {
        CXmlTree::CElt result = root.Clone();

        CHECK(result.m_pelt != nullptr);
        CHECK(result.m_pelt != root.m_pelt);
        CHECK(result.GetOwner() == &doc);
        CHECK(result.GetParent().IsEmpty());

        CHECK(result.GetAttr("attr-1") == "a");
        CHECK(result.GetNumChildren() == 1);

        CXmlTree::CElt cloneChild = result.GetFirstChild();
        CHECK(cloneChild.m_pelt != nullptr);
        CHECK(cloneChild.m_pelt != child.m_pelt);
        CHECK(cloneChild.GetOwner() == &doc);
        CHECK(cloneChild.GetName() == "Child");
        CHECK(cloneChild.GetAttr("attr-2") == "b");
    }

    SECTION("Clone child")
    {
        CXmlTree::CElt result = child.Clone();

        CHECK(result.m_pelt != nullptr);
        CHECK(result.m_pelt != child.m_pelt);
        CHECK(result.GetOwner() == &doc);
        CHECK(result.GetParent().IsEmpty());

        CHECK(result.GetAttr("attr-2") == "b");
        CHECK(result.GetNumChildren() == 0);
    }
}


TEST_CASE("CXmlTree::CFrag constructor tests")
{
    CXmlTree doc;
    doc.LoadStringA(R"(
<Root>
  <Child />
</Root>)");

    SECTION("CXmlTree:CFrag::CFrag()")
    {
        CXmlTree::CFrag frag;

        CHECK(frag.IsEmpty());

        CHECK_THROWS_AS(frag.GetFirstChild(), _com_error);
        CHECK_THROWS_AS(frag.DumpXML(), _com_error);
    }

    SECTION("CXmlTree::CFrag::CFrag(CXmlTree*)")
    {
        CXmlTree::CFrag frag{ &doc };

        CHECK(frag.IsEmpty());

        CXmlTree::CElt child = frag.GetFirstChild();
        CHECK(child.GetOwner() == &doc);
        
        CHECK(frag.DumpXML() == "");
    }
}

TEST_CASE("CXmlTree:CFrag - SaveKids")
{
    CXmlTree doc;
    doc.LoadStringA(R"(
<Root>
  <Child />
  <Child2>
    <Child2Child />
  </Child2>
</Root>)");

    doc.SetModified(false);


    SECTION("root")
    {
        CXmlTree::CFrag frag{ &doc };

        CXmlTree::CElt elem = doc.GetRoot();
        CXmlTree::CElt origElemChild1 = elem.GetFirstChild();

        frag.SaveKids(&elem);
        CHECK(doc.IsModified() == false);

        CXmlTree::CElt fragChild = frag.GetFirstChild();
        CXmlTree::CElt elemChild = elem.GetFirstChild();
        CHECK(fragChild.GetName() == "Child");
        CHECK(fragChild.GetNumChildren() == 0);
        CHECK(fragChild.m_pelt != elemChild.m_pelt);

        // note: SaveKids move the original elements to the
        // fragment, and re-inserts copies in the source doc
        CHECK(fragChild.m_pelt == origElemChild1.m_pelt);

        ++fragChild;
        ++elemChild;
        CHECK(fragChild.GetName() == "Child2");
        CHECK(fragChild.GetNumChildren() == 1);
        CHECK(fragChild.m_pelt != elemChild.m_pelt);

        CXmlTree::CElt fragChildChild = fragChild.GetFirstChild();
        CXmlTree::CElt elemChildChild = elemChild.GetFirstChild();
        CHECK(fragChildChild.GetName() == "Child2Child");
        CHECK(fragChildChild.GetNumChildren() == 0);
        CHECK(fragChildChild.m_pelt != elemChildChild.m_pelt);

        fragChildChild++;
        elemChildChild++;
        CHECK(fragChildChild.IsEmpty());
        CHECK(elemChildChild.IsEmpty());

        ++fragChild;
        ++elemChild;
        CHECK(fragChild.IsEmpty());
        CHECK(elemChild.IsEmpty());
    }

    SECTION("leaf")
    {
        CXmlTree::CFrag frag{ &doc };

        CXmlTree::CElt elem = doc.GetRoot().GetFirstChild();
        frag.SaveKids(&elem);
        CHECK(doc.IsModified() == false);

        CXmlTree::CElt fragElem = frag.GetFirstChild();
        CHECK(fragElem.IsEmpty());
    }
}

TEST_CASE("CXmlTree:CFrag - InsertKids")
{
    CXmlTree doc;
    doc.LoadStringA(R"(
<Root>
  <Existing />
</Root>)");

    doc.SetModified(false);

    CXmlTree::CElt root = doc.GetRoot();
    CXmlTree::CElt existing = root.GetFirstChild();

    SECTION("no elements")
    {
        CXmlTree::CFrag frag{ &doc };

        frag.InsertKids(&root);
        CHECK(doc.IsModified() == false);

        REQUIRE(root.GetNumChildren() == 1);
        CHECK(root.GetFirstChild().GetName() == "Existing");
    }


    SECTION("one element")
    {
        CXmlTree::CFrag frag{ &doc };

        CXmlTree::CElt fragRoot{ "Frag", &doc };
        CXmlTree::CElt fragRootChild = fragRoot.InsertChild(CXmlTree::CElt{ "New", &doc });
        fragRootChild.SetAttr("test-attr", "the value");
        frag.SaveKids(&fragRoot);

        frag.InsertKids(&root);
        CHECK(doc.IsModified());

        REQUIRE(root.GetNumChildren() == 2);

        CXmlTree::CElt elem = root.GetFirstChild();
        CHECK(elem.GetName() == "Existing");
        CHECK(elem.GetNumChildren() == 0);

        ++elem;
        CHECK(elem.GetName() == "New");
        CHECK(elem.GetNumChildren() == 0);
        CHECK(elem.GetAttr("test-attr") == "the value");

        ++elem;
        CHECK(elem.IsEmpty());
    }

    SECTION("one element before null")
    {
        CXmlTree::CFrag frag{ &doc };

        CXmlTree::CElt fragRoot{ "Frag", &doc };
        CXmlTree::CElt fragRootChild = fragRoot.InsertChild(CXmlTree::CElt{ "New", &doc });
        fragRootChild.SetAttr("test-attr", "the value");
        frag.SaveKids(&fragRoot);

        frag.InsertKids(&root, nullptr);
        CHECK(doc.IsModified());

        REQUIRE(root.GetNumChildren() == 2);

        CXmlTree::CElt elem = root.GetFirstChild();
        CHECK(elem.GetName() == "Existing");
        CHECK(elem.GetNumChildren() == 0);

        ++elem;
        CHECK(elem.GetName() == "New");
        CHECK(elem.GetNumChildren() == 0);
        CHECK(elem.GetAttr("test-attr") == "the value");

        ++elem;
        CHECK(elem.IsEmpty());
    }

    SECTION("one element before existing")
    {
        CXmlTree::CFrag frag{ &doc };

        CXmlTree::CElt fragRoot{ "Frag", &doc };
        CXmlTree::CElt fragRootChild = fragRoot.InsertChild(CXmlTree::CElt{ "New", &doc });
        fragRootChild.SetAttr("test-attr", "the value");
        frag.SaveKids(&fragRoot);

        frag.InsertKids(&root, &existing);
        CHECK(doc.IsModified());

        REQUIRE(root.GetNumChildren() == 2);

        CXmlTree::CElt elem = root.GetFirstChild();
        CHECK(elem.GetName() == "New");
        CHECK(elem.GetNumChildren() == 0);
        CHECK(elem.GetAttr("test-attr") == "the value");

        ++elem;
        CHECK(elem.GetName() == "Existing");
        CHECK(elem.GetNumChildren() == 0);

        ++elem;
        CHECK(elem.IsEmpty());
    }


    SECTION("one element with children")
    {
        CXmlTree::CFrag frag{ &doc };

        CXmlTree::CElt fragRoot{ "Frag", &doc };
        CXmlTree::CElt fragRootChild = fragRoot.InsertChild(CXmlTree::CElt{ "New", &doc });
        fragRootChild.InsertChild(CXmlTree::CElt{ "NewChild", &doc });
        frag.SaveKids(&fragRoot);

        frag.InsertKids(&root);
        CHECK(doc.IsModified());

        REQUIRE(root.GetNumChildren() == 2);

        CXmlTree::CElt elem = root.GetFirstChild();
        CHECK(elem.GetName() == "Existing");
        CHECK(elem.GetNumChildren() == 0);

        ++elem;
        CHECK(elem.GetName() == "New");
        CHECK(elem.GetNumChildren() == 1);

        CXmlTree::CElt elemChild = elem.GetFirstChild();
        CHECK(elemChild.GetName() == "NewChild");
        CHECK(elemChild.GetNumChildren() == 0);
    }

    SECTION("one element with children before null")
    {
        CXmlTree::CFrag frag{ &doc };

        CXmlTree::CElt fragRoot{ "Frag", &doc };
        CXmlTree::CElt fragRootChild = fragRoot.InsertChild(CXmlTree::CElt{ "New", &doc });
        fragRootChild.InsertChild(CXmlTree::CElt{ "NewChild", &doc });
        frag.SaveKids(&fragRoot);

        frag.InsertKids(&root, nullptr);
        CHECK(doc.IsModified());

        REQUIRE(root.GetNumChildren() == 2);

        CXmlTree::CElt elem = root.GetFirstChild();
        CHECK(elem.GetName() == "Existing");
        CHECK(elem.GetNumChildren() == 0);

        ++elem;
        CHECK(elem.GetName() == "New");
        CHECK(elem.GetNumChildren() == 1);

        CXmlTree::CElt elemChild = elem.GetFirstChild();
        CHECK(elemChild.GetName() == "NewChild");
        CHECK(elemChild.GetNumChildren() == 0);
    }

    SECTION("one element with children before existing")
    {
        CXmlTree::CFrag frag{ &doc };

        CXmlTree::CElt fragRoot{ "Frag", &doc };
        CXmlTree::CElt fragRootChild = fragRoot.InsertChild(CXmlTree::CElt{ "New", &doc });
        fragRootChild.InsertChild(CXmlTree::CElt{ "NewChild", &doc });
        frag.SaveKids(&fragRoot);

        frag.InsertKids(&root, &existing);
        CHECK(doc.IsModified());

        REQUIRE(root.GetNumChildren() == 2);

        CXmlTree::CElt elem = root.GetFirstChild();
        CHECK(elem.GetName() == "New");
        CHECK(elem.GetNumChildren() == 1);

        CXmlTree::CElt elemChild = elem.GetFirstChild();
        CHECK(elemChild.GetName() == "NewChild");
        CHECK(elemChild.GetNumChildren() == 0);

        ++elem;
        CHECK(elem.GetName() == "Existing");
        CHECK(elem.GetNumChildren() == 0);

        ++elem;
        CHECK(elem.IsEmpty());
    }


    SECTION("multiple elements")
    {
        CXmlTree::CFrag frag{ &doc };

        CXmlTree::CElt fragRoot{ "Frag", &doc };
        fragRoot.InsertChild(CXmlTree::CElt{ "New", &doc });
        fragRoot.InsertChild(CXmlTree::CElt{ "New2", &doc });
        frag.SaveKids(&fragRoot);

        frag.InsertKids(&root);
        CHECK(doc.IsModified());

        REQUIRE(root.GetNumChildren() == 3);

        CXmlTree::CElt elem = root.GetFirstChild();
        CHECK(elem.GetName() == "Existing");
        CHECK(elem.GetNumChildren() == 0);

        ++elem;
        CHECK(elem.GetName() == "New");
        CHECK(elem.GetNumChildren() == 0);

        ++elem;
        CHECK(elem.GetName() == "New2");
        CHECK(elem.GetNumChildren() == 0);
    }

    SECTION("multiple elements before null")
    {
        CXmlTree::CFrag frag{ &doc };

        CXmlTree::CElt fragRoot{ "Frag", &doc };
        fragRoot.InsertChild(CXmlTree::CElt{ "New", &doc });
        fragRoot.InsertChild(CXmlTree::CElt{ "New2", &doc });
        frag.SaveKids(&fragRoot);

        frag.InsertKids(&root, nullptr);
        CHECK(doc.IsModified());

        REQUIRE(root.GetNumChildren() == 3);

        CXmlTree::CElt elem = root.GetFirstChild();
        CHECK(elem.GetName() == "Existing");
        CHECK(elem.GetNumChildren() == 0);

        ++elem;
        CHECK(elem.GetName() == "New");
        CHECK(elem.GetNumChildren() == 0);

        ++elem;
        CHECK(elem.GetName() == "New2");
        CHECK(elem.GetNumChildren() == 0);
    }

    SECTION("multiple elements before existing")
    {
        CXmlTree::CFrag frag{ &doc };

        CXmlTree::CElt fragRoot{ "Frag", &doc };
        fragRoot.InsertChild(CXmlTree::CElt{ "New", &doc });
        fragRoot.InsertChild(CXmlTree::CElt{ "New2", &doc });
        frag.SaveKids(&fragRoot);

        frag.InsertKids(&root, &existing);
        CHECK(doc.IsModified());

        REQUIRE(root.GetNumChildren() == 3);

        CXmlTree::CElt elem = root.GetFirstChild();
        CHECK(elem.GetName() == "New");
        CHECK(elem.GetNumChildren() == 0);

        ++elem;
        CHECK(elem.GetName() == "New2");
        CHECK(elem.GetNumChildren() == 0);

        ++elem;
        CHECK(elem.GetName() == "Existing");
        CHECK(elem.GetNumChildren() == 0);

        ++elem;
        CHECK(elem.IsEmpty());
    }
}

TEST_CASE("CXmlTree::CFrag - Append/InsertClone")
{
    CXmlTree doc;
    doc.LoadStringA(R"(
<Root>
  <Existing />
</Root>)");

    doc.SetModified(false);

    CXmlTree::CElt root = doc.GetRoot();

    CXmlTree::CFrag frag{ &doc };


    SECTION("append empty element")
    {
        CXmlTree::CElt elem{ "Test", &doc };

        CXmlTree::CElt result = frag.AppendClone(elem);

        CXmlTree::CElt fragElem = frag.GetFirstChild();

        REQUIRE(result.m_pelt != nullptr);
        CHECK(result.m_pelt != elem.m_pelt);
        CHECK(result.m_pelt == fragElem.m_pelt);

        CHECK(result.GetName() == "Test");
        CHECK(result.GetOwner() == &doc);

        fragElem++;
        CHECK(fragElem.IsEmpty());
    }

    SECTION("insert empty element before null")
    {
        CXmlTree::CElt elem{ "Test", &doc };

        CXmlTree::CElt result = frag.InsertClone(elem, nullptr);

        CXmlTree::CElt fragElem = frag.GetFirstChild();

        REQUIRE(result.m_pelt != nullptr);
        CHECK(result.m_pelt != elem.m_pelt);
        CHECK(result.m_pelt == fragElem.m_pelt);

        CHECK(result.GetName() == "Test");
        CHECK(result.GetOwner() == &doc);

        fragElem++;
        CHECK(fragElem.IsEmpty());
    }

    SECTION("insert empty element before existing")
    {
        CXmlTree::CElt existing = frag.AppendClone(CXmlTree::CElt{ "Existing", &doc });

        CXmlTree::CElt elem{ "Test", &doc };

        CXmlTree::CElt result = frag.InsertClone(elem, &existing);

        CXmlTree::CElt fragElem = frag.GetFirstChild();

        REQUIRE(result.m_pelt != nullptr);
        CHECK(result.m_pelt != elem.m_pelt);
        CHECK(result.m_pelt == fragElem.m_pelt);

        CHECK(result.GetName() == "Test");
        CHECK(result.GetOwner() == &doc);

        fragElem++;
        CHECK(fragElem.GetName() == "Existing");
        CHECK(fragElem.m_pelt == existing.m_pelt);

        fragElem++;
        CHECK(fragElem.IsEmpty());
    }


    SECTION("one element with children")
    {
        CXmlTree::CElt elem{ "Test", &doc };
        elem.InsertChild(CXmlTree::CElt{ "Child", &doc });
        elem.SetAttr("test-attr-1", "the value");

        CXmlTree::CElt result = frag.AppendClone(elem);

        CXmlTree::CElt fragElem = frag.GetFirstChild();

        REQUIRE(result.m_pelt != nullptr);
        CHECK(result.m_pelt != elem.m_pelt);
        CHECK(result.m_pelt == fragElem.m_pelt);

        CHECK(result.GetName() == "Test");
        CHECK(result.GetOwner() == &doc);
        CHECK(result.GetAttr("test-attr-1") == "the value");
        CHECK(result.GetNumChildren() == 1);

        CXmlTree::CElt child = result.GetFirstChild();
        CHECK(child.GetName() == "Child");

        fragElem++;
        CHECK(fragElem.IsEmpty());
    }

    SECTION("one element with children before null")
    {
        CXmlTree::CElt elem{ "Test", &doc };
        elem.InsertChild(CXmlTree::CElt{ "Child", &doc });
        elem.SetAttr("test-attr-1", "the value");

        CXmlTree::CElt result = frag.InsertClone(elem, nullptr);

        CXmlTree::CElt fragElem = frag.GetFirstChild();

        REQUIRE(result.m_pelt != nullptr);
        CHECK(result.m_pelt != elem.m_pelt);
        CHECK(result.m_pelt == fragElem.m_pelt);

        CHECK(result.GetName() == "Test");
        CHECK(result.GetOwner() == &doc);
        CHECK(result.GetAttr("test-attr-1") == "the value");
        CHECK(result.GetNumChildren() == 1);

        CXmlTree::CElt child = result.GetFirstChild();
        CHECK(child.GetName() == "Child");

        fragElem++;
        CHECK(fragElem.IsEmpty());
    }

    SECTION("one element with children before existing")
    {
        CXmlTree::CElt existing = frag.AppendClone(CXmlTree::CElt{ "Existing", &doc });

        CXmlTree::CElt elem{ "Test", &doc };
        CXmlTree::CElt elemChild = elem.InsertChild(CXmlTree::CElt{ "Child", &doc });
        elem.SetAttr("test-attr-1", "the value");

        CXmlTree::CElt result = frag.InsertClone(elem, &existing);

        CXmlTree::CElt fragElem = frag.GetFirstChild();

        REQUIRE(result.m_pelt != nullptr);
        CHECK(result.m_pelt != elem.m_pelt);
        CHECK(result.m_pelt == fragElem.m_pelt);

        CHECK(result.GetName() == "Test");
        CHECK(result.GetOwner() == &doc);
        CHECK(result.GetAttr("test-attr-1") == "the value");
        CHECK(result.GetNumChildren() == 1);

        CXmlTree::CElt child = result.GetFirstChild();
        CHECK(child.m_pelt != nullptr);
        CHECK(child.m_pelt != elemChild.m_pelt);
        CHECK(child.GetName() == "Child");

        ++fragElem;
        CHECK(fragElem.GetName() == "Existing");

        ++fragElem;
        CHECK(fragElem.IsEmpty());
    }
}
