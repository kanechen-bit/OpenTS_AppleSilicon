#!/usr/bin/env python3
"""
Read resources out of a Win32 PE module.

Tiberian Sun keeps every dialog template and every localised string in
Language.dll -- a 32-bit PE whose only payload is a .rsrc section. The engine
reaches them through FindResource / LoadResource / LoadString, so a port that
cannot read the resource directory has no dialogs and no text at all.

This is the ground truth the port's own reader (code/port/port_resource.cpp) is
checked against. It is deliberately independent: it parses the file from the
format descriptions rather than sharing any code with the C++.

    peres.py summary   <dll>
    peres.py strings   <dll> [first] [count]
    peres.py dialogs   <dll>
    peres.py template  <dll> <dialog-id>
    peres.py allstrings <dll>
"""

import argparse
import struct
import sys


# ---------------------------------------------------------------------------
# Container
# ---------------------------------------------------------------------------

RT = {
    1: "CURSOR", 2: "BITMAP", 3: "ICON", 4: "MENU", 5: "DIALOG", 6: "STRING",
    7: "FONTDIR", 8: "FONT", 9: "ACCELERATOR", 10: "RCDATA", 11: "MESSAGETABLE",
    12: "GROUP_CURSOR", 14: "GROUP_ICON", 16: "VERSION", 24: "MANIFEST",
}

RT_DIALOG = 5
RT_STRING = 6


class PE:
    def __init__(self, path):
        self.path = path
        self.data = open(path, "rb").read()
        d = self.data

        if d[:2] != b"MZ":
            raise ValueError("not a PE image (no MZ)")
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        if d[pe:pe + 4] != b"PE\0\0":
            raise ValueError("not a PE image (no PE signature)")

        self.machine, self.nsections = struct.unpack_from("<HH", d, pe + 4)
        opt_size = struct.unpack_from("<H", d, pe + 20)[0]
        opt = pe + 24
        self.magic = struct.unpack_from("<H", d, opt)[0]
        if self.magic != 0x10B:
            raise ValueError("expected PE32 (0x10B), got 0x%X" % self.magic)

        # PE32 data directories start at optional header offset 96.
        self.dirs = [
            struct.unpack_from("<II", d, opt + 96 + i * 8)
            for i in range(16)
        ]
        self.res_rva, self.res_size = self.dirs[2]

        secs = []
        at = opt + opt_size
        for i in range(self.nsections):
            o = at + i * 40
            name = d[o:o + 8].rstrip(b"\0").decode("latin1")
            vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", d, o + 8)
            secs.append((name, vaddr, vsize, rawptr, rawsize))
        self.sections = secs

        if self.res_rva == 0:
            raise ValueError("image carries no resource directory")

        # The resource directory entries store RVAs; section-relative is not
        # guaranteed, so translate properly.
        self.res_base = self.off(self.res_rva)

    def off(self, rva):
        for name, vaddr, vsize, rawptr, rawsize in self.sections:
            if vaddr <= rva < vaddr + max(vsize, rawsize):
                return rawptr + (rva - vaddr)
        raise ValueError("rva 0x%X is not in any section" % rva)

    # -- directory walk ----------------------------------------------------

    def _entries(self, off):
        """(name_or_id, is_directory, offset, is_named) for one directory."""
        d = self.data
        n_named, n_id = struct.unpack_from("<HH", d, off + 12)
        out = []
        at = off + 16
        for i in range(n_named + n_id):
            name, sub = struct.unpack_from("<II", d, at + i * 8)
            out.append((
                name & 0x7FFFFFFF,
                bool(sub & 0x80000000),
                sub & 0x7FFFFFFF,
                bool(name & 0x80000000),
            ))
        return out

    def types(self):
        """{type: {id_or_name: [(lang, data_rva, size)]}} for the whole tree."""
        found = {}
        for tid, is_dir, sub, _named in self._entries(self.res_base):
            if not is_dir:
                continue
            names = {}
            for rid, is_dir2, sub2, named2 in self._entries(self.res_base + sub):
                if not is_dir2:
                    continue
                langs = []
                for lang, is_dir3, sub3, _n in self._entries(self.res_base + sub2):
                    if is_dir3:
                        continue
                    rva, size = struct.unpack_from("<II", self.data, self.res_base + sub3)
                    langs.append((lang, rva, size))
                names[rid] = langs
            found[tid] = names
        return found

    def find(self, type_id, name_id):
        """First (lang, bytes) for a resource, or None."""
        for tid, is_dir, sub, _ in self._entries(self.res_base):
            if tid != type_id or not is_dir:
                continue
            for rid, is_dir2, sub2, _ in self._entries(self.res_base + sub):
                if rid != name_id or not is_dir2:
                    continue
                for lang, is_dir3, sub3, _ in self._entries(self.res_base + sub2):
                    if is_dir3:
                        continue
                    rva, size = struct.unpack_from("<II", self.data, self.res_base + sub3)
                    return lang, self.data[self.off(rva):self.off(rva) + size]
        return None

    # -- strings -----------------------------------------------------------

    def string_block(self, block_id):
        """
        Reads one STRINGTABLE block into a list of 16 string-or-None.

        The documented layout puts sixteen length words first and the text
        after them. Every block in every one of the four shipped Language.dll
        files uses a *different* layout: length word, text, length word, text.
        See the note on `documented_layout` below -- the measured layout is the
        one the game's own strings come out of, so it is the one used here.
        """
        entry = self.find(RT_STRING, block_id)
        if entry is None:
            return None
        _lang, blob = entry

        out = self._interleaved(blob)
        if out is not None:
            return out
        return self._documented(blob)

    @staticmethod
    def _interleaved(blob):
        """length, text, length, text ... -- what the shipped files use."""
        out = []
        at = 0
        for _ in range(16):
            if at + 2 > len(blob):
                return None
            n = struct.unpack_from("<H", blob, at)[0]
            at += 2
            if at + n * 2 > len(blob):
                return None
            text = blob[at:at + n * 2].decode("utf-16-le", errors="replace")
            at += n * 2
            out.append(text if n else None)
        # An exact fit is what makes this the right reading rather than a
        # coincidence; both other checks would pass on a truncated blob.
        return out if at == len(blob) else None

    @staticmethod
    def _documented(blob):
        """The layout MSDN describes: sixteen lengths, then the text."""
        if len(blob) < 32:
            return None
        lengths = struct.unpack_from("<16H", blob, 0)
        if 32 + sum(lengths) * 2 != len(blob):
            return None
        out = []
        at = 32
        for n in lengths:
            text = blob[at:at + n * 2].decode("utf-16-le", errors="replace")
            at += n * 2
            out.append(text if n else None)
        return out

    def load_string(self, string_id):
        """
        LoadString's lookup: the tables hold 16 strings each and are numbered
        from one, so the block is (id >> 4) + 1 and the index is id & 15.
        """
        block = self.string_block((string_id >> 4) + 1)
        if block is None:
            return None
        return block[string_id & 15]


# ---------------------------------------------------------------------------
# Dialog templates
# ---------------------------------------------------------------------------

# Header styles that change how the template is laid out.
DS_SETFONT = 0x0040


def _sz(blob, at):
    """Reads a NUL-terminated UTF-16 string, returns (text, next offset)."""
    end = at
    while end + 1 < len(blob) and blob[end:end + 2] != b"\0\0":
        end += 2
    return blob[at:end].decode("utf-16-le", errors="replace"), end + 2


def _align(n, to):
    return (n + (to - 1)) & ~(to - 1)


def _cls_or_ordinal(blob, at):
    """
    A menu, class or title field: 0x0000 for absent, 0xFFFF followed by an
    ordinal, otherwise a UTF-16 string.
    """
    first = struct.unpack_from("<H", blob, at)[0]
    if first == 0x0000:
        return None, at + 2
    if first == 0xFFFF:
        return struct.unpack_from("<H", blob, at + 2)[0], at + 4
    return _sz(blob, at)


# Control style bits we care about when naming things for a human.
BS_TYPES = {
    0x0000: "BS_PUSHBUTTON", 0x0001: "BS_DEFPUSHBUTTON", 0x0002: "BS_CHECKBOX",
    0x0003: "BS_AUTOCHECKBOX", 0x0004: "BS_RADIOBUTTON", 0x0005: "BS_3STATE",
    0x0006: "BS_AUTO3STATE", 0x0007: "BS_GROUPBOX", 0x0008: "BS_USERBUTTON",
    0x0009: "BS_AUTORADIOBUTTON", 0x000A: "BS_PUSHBOX", 0x000B: "BS_OWNERDRAW",
}


def parse_template(blob):
    """
    Decodes a DLGTEMPLATE or DLGTEMPLATEEX into a dict. Returns
    (template, next_offset) so callers can report how much was consumed.
    """
    at = 0
    dlg_ver, signature = struct.unpack_from("<HH", blob, at)

    if dlg_ver == 1 and signature == 0xFFFF:
        ex = True
        help_id, ex_style, style = struct.unpack_from("<III", blob, at + 4)
        cdit = struct.unpack_from("<H", blob, at + 16)[0]
        x, y, cx, cy = struct.unpack_from("<hhhh", blob, at + 18)
        at += 26
    else:
        ex = False
        style, ex_style = struct.unpack_from("<II", blob, at)
        cdit = struct.unpack_from("<H", blob, at + 8)[0]
        x, y, cx, cy = struct.unpack_from("<hhhh", blob, at + 10)
        help_id = 0
        at += 18

    menu, at = _cls_or_ordinal(blob, at)
    class_atom, at = _cls_or_ordinal(blob, at)
    title, at = _sz(blob, at)

    font = None
    if style & DS_SETFONT:
        pointsize = struct.unpack_from("<H", blob, at)[0]
        at += 2
        weight = 0
        italic = 0
        charset = 0
        if ex:
            weight, italic, charset = struct.unpack_from("<HBB", blob, at)
            at += 4
        typeface, at = _sz(blob, at)
        font = {"size": pointsize, "weight": weight, "italic": italic,
                "charset": charset, "face": typeface}

    at = _align(at, 4)

    items = []
    for index in range(cdit):
        # Items are DWORD aligned, but the padding that would follow the last
        # one is not part of the resource -- the block simply ends. Applying
        # the alignment at the top of each item reproduces that, and is the
        # difference between consuming a template exactly and overrunning it
        # by two bytes on the 38 dialogs whose final item ends unaligned.
        if index > 0:
            at = _align(at, 4)

        if ex:
            ihelp, iex, istyle = struct.unpack_from("<III", blob, at)
            ix, iy, icx, icy = struct.unpack_from("<hhhh", blob, at + 12)
            iid = struct.unpack_from("<I", blob, at + 20)[0]
            at += 24
        else:
            istyle, iex = struct.unpack_from("<II", blob, at)
            ix, iy, icx, icy = struct.unpack_from("<hhhh", blob, at + 8)
            iid = struct.unpack_from("<H", blob, at + 16)[0]
            ihelp = 0
            at += 18

        icls, at = _cls_or_ordinal(blob, at)
        itxt, at = _cls_or_ordinal(blob, at)

        data_size = struct.unpack_from("<H", blob, at)[0]
        at += 2
        creation = blob[at:at + data_size]
        at += data_size

        items.append({
            "id": iid, "class": icls, "title": itxt, "style": istyle,
            "exstyle": iex, "help": ihelp, "creation": creation,
            "x": ix, "y": iy, "cx": icx, "cy": icy,
        })

    return {
        "ex": ex, "style": style, "exstyle": ex_style, "help": help_id,
        "cdit": cdit, "x": x, "y": y, "cx": cx, "cy": cy,
        "menu": menu, "class": class_atom, "title": title, "font": font,
        "items": items,
    }, at


def style_names(s):
    out = []
    if s & 0x80000000:
        out.append("WS_POPUP")
    if s & 0x40000000:
        out.append("WS_CHILD")
    if s & 0x10000000:
        out.append("WS_VISIBLE")
    if s & 0x08000000:
        out.append("WS_DISABLED")
    if s & 0x02000000:
        out.append("WS_CLIPSIBLINGS")
    if s & 0x00800000:
        out.append("WS_BORDER")
    if s & 0x00010000:
        out.append("WS_TABSTOP")
    if s & 0x00020000:
        out.append("WS_GROUP")
    return out


# Predefined class atoms a template may name instead of a string. These are
# what the engine's InitializeCtrl compares against, so they matter.
CLASS_ATOMS = {
    0x0080: "Button", 0x0081: "Edit", 0x0082: "Static",
    0x0083: "ListBox", 0x0084: "ScrollBar", 0x0085: "ComboBox",
}


def class_name(c):
    if isinstance(c, int):
        return CLASS_ATOMS.get(c, "atom 0x%04X" % c)
    return c


def dialog_style_names(s):
    out = style_names(s)
    for bit, name in ((0x0040, "DS_SETFONT"), (0x0080, "DS_MODALFRAME"),
                      (0x0004, "DS_3DLOOK"), (0x0008, "DS_FIXEDSYS"),
                      (0x0800, "DS_CENTER"), (0x0400, "DS_CONTROL"),
                      (0x0001, "DS_ABSALIGN"), (0x0002, "DS_SYSMODAL")):
        if s & bit:
            out.append(name)
    return out


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def cmd_summary(args):
    pe = PE(args.dll)
    print("%s: PE32 machine=0x%04X sections=%d" % (pe.path, pe.machine, pe.nsections))
    print("resource directory rva=0x%X size=%d, file offset 0x%X"
          % (pe.res_rva, pe.res_size, pe.res_base))
    print()
    tree = pe.types()
    print("%-16s %s" % ("type", "resources"))
    for tid in sorted(tree):
        print("%-16s %d" % (RT.get(tid, "RT_%d" % tid), len(tree[tid])))
    return 0


def cmd_strings(args):
    pe = PE(args.dll)
    first = args.first
    count = args.count
    bad = 0
    for sid in range(first, first + count):
        s = pe.load_string(sid)
        shown = "None" if s is None else repr(s)
        print("  id=%-6d %s" % (sid, shown))
        if s is None:
            bad += 1
    print()
    print("block = (id >> 4) + 1 ; index = id & 15")
    print("absent in this range: %d of %d" % (bad, count))
    return 0


def cmd_allstrings(args):
    pe = PE(args.dll)
    tree = pe.types()
    blocks = sorted(tree.get(RT_STRING, {}))
    print("string tables: %s" % blocks)
    total = 0
    present = 0
    for block in blocks:
        for i in range(16):
            sid = (block - 1) * 16 + i
            total += 1
            if pe.load_string(sid) is not None:
                present += 1
    print("ids covered: %d, with text: %d" % (total, present))
    return 0


def cmd_dialogs(args):
    pe = PE(args.dll)
    tree = pe.types()
    found = sorted(tree.get(RT_DIALOG, {}))
    print("%d dialog templates" % len(found))
    for rid in found:
        entry = pe.find(RT_DIALOG, rid)
        if entry is None:
            continue
        lang, blob = entry
        try:
            tmpl, used = parse_template(blob)
        except Exception as exc:
            print("  id=%-6d size=%-6d lang=0x%04X  PARSE FAILED: %s"
                  % (rid, len(blob), lang, exc))
            continue
        print("  id=%-6d size=%-6d lang=0x%04X  %s items=%-3d %dx%d %r | consumed %d/%d"
              % (rid, len(blob), lang, "EX " if tmpl["ex"] else "std",
                 tmpl["cdit"], tmpl["cx"], tmpl["cy"], tmpl["title"],
                 used, len(blob)))
    return 0


def cmd_template(args):
    pe = PE(args.dll)
    entry = pe.find(RT_DIALOG, args.id)
    if entry is None:
        print("no dialog %d" % args.id)
        return 1
    lang, blob = entry
    tmpl, used = parse_template(blob)
    print("dialog %d: lang=0x%04X size=%d consumed=%d (%s)"
          % (args.id, lang, len(blob), used,
             "exact" if used == len(blob) else "MISMATCH"))
    print("  format : %s" % ("DLGTEMPLATEEX" if tmpl["ex"] else "DLGTEMPLATE"))
    print("  title  : %r" % tmpl["title"])
    print("  class  : %r  menu: %r" % (tmpl["class"], tmpl["menu"]))
    print("  style  : 0x%08X  %s" % (tmpl["style"], " ".join(dialog_style_names(tmpl["style"]))))
    print("  exstyle: 0x%08X" % tmpl["exstyle"])
    print("  rect   : (%d,%d) %dx%d dialog units" % (tmpl["x"], tmpl["y"], tmpl["cx"], tmpl["cy"]))
    if tmpl["font"]:
        f = tmpl["font"]
        print("  font   : %r %dpt weight=%d italic=%d charset=%d"
              % (f["face"], f["size"], f["weight"], f["italic"], f["charset"]))
    print("  items  : %d" % tmpl["cdit"])
    for i, it in enumerate(tmpl["items"]):
        print("    [%d] id=%-5s class=%-18s style=0x%08X %dx%d at (%d,%d) %r%s"
              % (i, it["id"], class_name(it["class"]), it["style"],
                 it["cx"], it["cy"], it["x"], it["y"], it["title"],
                 " creation=%d bytes" % len(it["creation"]) if it["creation"] else ""))
        print("         %s" % " ".join(style_names(it["style"])))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("summary", help="PE and resource-type overview")
    p.add_argument("dll")
    p.set_defaults(func=cmd_summary)

    p = sub.add_parser("strings", help="load specific string ids")
    p.add_argument("dll")
    p.add_argument("first", type=int, nargs="?", default=0)
    p.add_argument("count", type=int, nargs="?", default=32)
    p.set_defaults(func=cmd_strings)

    p = sub.add_parser("allstrings", help="how many string ids carry text")
    p.add_argument("dll")
    p.set_defaults(func=cmd_allstrings)

    p = sub.add_parser("dialogs", help="list every dialog template")
    p.add_argument("dll")
    p.set_defaults(func=cmd_dialogs)

    p = sub.add_parser("template", help="decode one dialog template in full")
    p.add_argument("dll")
    p.add_argument("id", type=int)
    p.set_defaults(func=cmd_template)

    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
