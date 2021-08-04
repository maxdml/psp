import yaml
import os

class YmlImportException(Exception):
    pass

#https://stackoverflow.com/questions/528281/how-can-i-include-an-yaml-file-inside-another
class IncludeLoader(yaml.SafeLoader):
    ''' A yaml loader that adds the "!include", "!import", and "!inherit |" operators '''

    included_files = []

    def __init__(self, stream):
        super(IncludeLoader, self).__init__(stream)
        try :
            self._root = os.path.split(stream.name)[0]
        except AttributeError:
            self._root = ''

    def do_include(self, node):
        filename = os.path.join(self._root, self.construct_scalar(node))
        with open(filename, 'r') as f:
            rtn = yaml.load(f, IncludeLoader)
            self.included_files.append(filename)
            return rtn

    def load_import(self, import_str):
        split_import = import_str.split('::')
        filename = os.path.join(self._root, split_import[0])
        with open(filename, 'r') as f:
            rtn = yaml.load(f, IncludeLoader)
            self.included_files.append(filename)
            for i, sub_node in enumerate(split_import[1:]):
                if sub_node in rtn:
                    rtn = rtn[sub_node]
                else:
                    raise YmlImportException("Could not find {} in {}".format(':'.join(split_import[1:i+2]), filename))
            return rtn

    def do_import(self, node):
        import_str = self.construct_scalar(node)
        return self.load_import(import_str)

    @classmethod
    def merge_dicts(cls, d1, d2):
        d3 = d1.copy()
        for k, v in d2.items():
            if k in d3:
                if isinstance(d3[k], dict):
                    d3[k] = cls.merge_dicts(d3[k], v)
                else:
                    d3[k] = v
            else:
                d3[k] = v
        return d3

    def do_inherit(self, node):
        import_str = self.construct_scalar(node)
        lines = import_str.splitlines()
        import_str = lines[0]
        rest = '\n'.join(lines[1:])

        imported = self.load_import(import_str)
        merger = yaml.load(rest, IncludeLoader)

        if merger is not None:
            imported = self.merge_dicts(imported, merger)
        return imported

IncludeLoader.add_constructor('!include', IncludeLoader.do_include)
IncludeLoader.add_constructor('!import', IncludeLoader.do_import)
IncludeLoader.add_constructor("!inherit", IncludeLoader.do_inherit)
