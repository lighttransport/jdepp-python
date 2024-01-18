# From jdepp/to_tree.py ---------------
# Copyright: Naoki Yoshinaga <ynaga@tkl.iis.u-tokyo.ac.jp>
# License: BSD/GPLv2/LGPLv2.1

# customizable parameters
indent = prob and 4 or 3 # for one dependency arc
offset = 2               # offset from the left

class Binfo:
    """ bunsetsu infomation """
    def __init__ (self, *args):
        self.id, self.head, self.prob, self.fail, self.gold = args
        self.morph, self.len, self.depth, self.first_child = "", 0, 0, -1
    def str    (self)         : return self.morph + (self.len % 2 and " " or "")
    def width  (self)         : return (self.len + 1) / 2;
    def offset (self, offset) : return offset - self.width () - self.depth


def treeify (binfo):
    tree = ''
    for c in reversed (binfo[:-1]):
        c.depth = binfo[c.head].depth + indent
        binfo[c.head].first_child = c.id
    width = offset + max (b.width () + b.depth for b in binfo) # tree width
    for b in binfo:
        if b.head == -1:
            if b.id != len (binfo) - 1:
                return "no head information; chunking output seems missing?"
            tree += "% 3d:%s%s" % (b.id, "　" * int (b.offset (width)), b.str ())
        else:
            if b.fail:
                tree += b.head > int (b.gold[:-1]) and "\033[31m" or "\033[36m"
            tree += "% 3d:%s%s" % (b.id, u"　" * int (b.offset (width)), b.str ())
            h = binfo[b.head]
            tree += "━" * (b.depth - h.depth - (b.prob and 3 or 1))
            tree += b.prob # "─"
            tree += b.id == h.first_child and "┓" or "┫" # "┐" or "┤"
            tree += b.fail and "%-4s\033[0m" % b.gold or ""
            while h.head != -1: # draw arcs spanning from x < b to y > h
                c, h = h, binfo[h.head]
                tree += "　" * (c.depth - h.depth - (b.fail and 3 or 1))
                tree += h.first_child < b.id and "┃" or "　" # "│" or "　"
                b.fail = False
            tree += "\n"
    return tree

# Export as dot(graphviz)
def dottify(graph_name: str = 'jdepp', label_name: str = 'dependency'):

    s = ''

    s += 'digraph ' + graph_name + '{\n'

    s += '\ngraph [\n'
    s += '  charset = "UTF-8";\n'
    s += '  label = "{}";\n'.format(label_name)
    s += '];\n'
    s += '\n'

    s += 'node [\n'

    s += ']\n'

    s += '}\n'

    return s
