import ctypes

def from_c_string2(addr, length):
    """utf-8 decode a C string into a python string"""
    return str(ctypes.string_at(addr, length), encoding='utf-8')
def from_c_string(addr):
    """utf-8 decode a C string into a python string"""
    return str(addr, encoding='utf-8')
def cstr(s):
    """convenience function to encode a str as bytes"""
    return s.encode('utf-8')

def BoolProperty(c_getter, c_setter):
    def getter(self):
        return True if c_getter(self._ptr) != 0 else False
    def setter(self, value):
        return c_setter(self._ptr, value)
    return property(getter, setter)

def IntProperty(c_getter, c_setter):
    def getter(self):
        return int(c_getter(self._ptr))
    def setter(self, value):
        return c_setter(self._ptr, value)
    return property(getter, setter)

def IntGetter(getter):
    def getter(self):
        return int(getter(self._ptr))
    return property(getter)

def StringProperty(c_getter, c_setter):
    def getter(self):
        return from_c_string(c_getter(self._ptr))
    def setter(self, value):
        return c_setter(self._ptr, cstr(value))
    return property(getter, setter)

class StringListAccess(object):
    def __init__(self, owner, count, get, add, del_s, del_i, set_i=None,
                 conv=cstr):
        self.owner   = owner
        self._conv   = conv
        self._count  = count
        self._get    = get
        self._add    = add
        self._del_s  = del_s
        self._del_i  = del_i
        if set_i is None:
            self.__setitem__ = None
        else:
            self.__set_i  = set_i

    def __len__(self):
        return self._count(self.owner._ptr)

    def __getitem__(self, key):
        if isinstance(key, slice):
            return self.__getslice__(self, key.start, key.stop, key.step)
        if not isinstance(key, int): raise TypeError
        if key < 0: raise IndexError
        count = self.__len__()
        if key >= count: raise IndexError
        return self.get(key, 1)[0]

    def __getslice__(self, start=None, stop=None, step=None):
        step  = step or 1
        start = start or 0
        count = stop - start if stop else None
        if step == 0:
            raise ValueError('step cannot be zero')
        if count == 0:
            return []
        if step > 0:
            if count < 0:
                return []
            values = self.get(start, count)
            return values[::step]
        else:
            if count > 0:
                return []
            values = self.get(start+count, -count)
            return values[-count:0:step]

    def __setitem__(self, key, value):
        if isinstance(key, slice):
            return self.__setslice__(key.start, key.stop, value, key.step)
        if not isinstance(key, int): raise TypeError
        if key < 0: raise IndexError
        count = self.__len__()
        if key > count:
            raise IndexError
        elif key == count:
            self.add(value)
        else:
            self.set_i(key, value)

    def __setslice__(self, start, stop, values, step=None):
        count = len(self)
        start = start or 0
        stop  = stop  or count
        step  = step  or 1
        if step == 0:
            raise ValueError('step cannot be zero')
        if step > 0:
            if start < 0:
                raise IndexError
            for v in values:
                if start >= stop:
                    return
                if start == count:
                    if self.add(v):
                        count += 1
                elif start > count:
                    raise IndexError
                else:
                    self.set_i(start, v)
                start += step
        else:
            for v in values:
                if start <= stop:
                    return
                if start < 0:
                    raise IndexError
                if start == count:
                    if self.add(v):
                        count += 1
                elif start > count:
                    raise IndexError
                else:
                    self.set_i(start, v)
                start += step

    def __delitem__(self, key):
        if isinstance(key, slice):
            return self.__delslice__(self, key.start, key.stop, key.step)
        self.delete(key)

    def __delslice__(self, start=None, stop=None, step=None):
        step  = step or 1
        start = start or 0
        stop  = stop   or self.count()
        if step == 0:
            raise ValueError('step cannot be zero')
        if step > 0:
            s = slice(start, stop, step)
        else:
            s = reverse(slice(start, stop, step))
        raise Excpetion('TODO')

    def __contains__(self, value):
        return value in self.get()

    class Iterator(object):
        def __init__(self, owner, beg, end):
            self.owner = owner
            self.index = beg
            self.count = end
        def __iter__(self):
            return self
        def next(self):
            if self.index >= self.count:
                raise StopIteration
            v = self.owner[self.index]
            self.index += 1
            return v
        def __next__(self):
            return self.next()
    def __iter__(self):
        return self.Iterator(self, 0, len(self))
    def __reversed__(self):
        return self.Iterator(self, len(self)-1, -1)

    def get(self, offset=None, count=None):
        avail = len(self)
        if offset is None:
            offset = 0
            if count is None:
                count = avail
        elif count is None:
            count = 1
        if count > avail - offset:
            count = avail - offset
        lst = (ctypes.c_char_p * count)()
        got = self._get(self.owner._ptr, lst, offset, count)
        return [ from_c_string(x) for x in lst[0:got] ]

    def add(self, s):
        return True if self._add(self.owner._ptr, self._conv(s)) else False

    def set_i(self, index, value):
        return (
            True if self.__set_i(self.owner._ptr, index, self._conv(value))
            else False)

    def delete(self, what):
        if isinstance(what, str):
            if 0 == self._del_s(self.owner._ptr, self._conv(what)):
                raise IndexError
        elif isinstance(what, int):
            if 0 == self._del_i(self.owner._ptr, what):
                raise IndexError
        else:
            raise KeyError('invalid type for delete operation: %s (%s)' %
                           (type(what), str(what)))

    def append(self, value):
        return self.add(value)
    def extend(self, value):
        for i in value:
            self.append(value)

class DepListAccess(StringListAccess):
    def __init__(self, owner, count, get, add, del_s, del_t, del_i, set_i=None,
                 conv=cstr):
        StringListAccess.__init__(self, owner, count, get, add, del_s, del_i,
                                  set_i, conv)
        self._del_t = del_t

    def add(self, s):
        if isinstance(s, str):
            x = s.find('<')
            x = x if x >= 0 else s.find('>')
            x = x if x >= 0 else s.find('=')
            x = x if x >= 0 else s.find('!')
            if x == -1:
                return (True if self._add(self.owner._ptr,
                                          self._conv(s),
                                          self._conv("")) == 1
                        else False)
            else:
                a = s[0:x]
                b = s[x+1 if s[x+1] != '=' else x+2:]
                return (True if self._add(self.owner._ptr,
                                          self._conv(a),
                                          self._conv(b)) == 1
                        else False)
        elif isinstance(s, (str,str)):
            return (True if self._add(self.owner._ptr,
                                      self._conv(s[0]),
                                      self._conv(s[1])) == 1
                    else False)

    def get(self, offset=None, count=None):
        avail = self._count(self.owner._ptr)
        if offset is None:
            offset = 0
            if count is None:
                count = avail
        elif count is None:
            count = 1
        if count > avail - offset:
            count = avail - offset
        lst_a = (ctypes.c_char_p * count)()
        lst_b = (ctypes.c_char_p * count)()
        got = self._get(self.owner._ptr, lst_a, lst_b, offset, count)
        return zip([ from_c_string(x) for x in lst_a[0:got] ],
                   [ from_c_string(x) for x in lst_b[0:got] ])

    def delete(self, what):
        if isinstance(what, str):
            if 0 == self._del_s(self.owner._ptr, self._conv(what)):
                raise IndexError
        elif isinstance(what, int):
            if 0 == self._del_i(self.owner._ptr, what):
                raise IndexError
        elif isinstance(what, (str,str)):
            if 0 == self._del_t(self.owner._ptr, self._conv(what[0]),
                                self._conv(what[1])):
                raise IndexError
        else:
            raise KeyError('invalid type for delete operation: %s (%s)' %
                           (type(what), str(what)))

class StringMapOfStringList(object):
    def __init__(self, owner, count_keys, get_keys, count_values, get_values,
                 add, del_kv, del_ki, set_ki, contains_key):
        self.owner    = owner
        self.__count_keys   = count_keys
        self.__get_keys     = get_keys
        self.__count_values = count_values
        self.__get_values   = get_values
        self.__add          = add
        self.__del_kv       = del_kv
        self.__del_ki       = del_ki
        self.__set_ki       = set_ki
        self.__contains     = contains_key

    def __len__(self):
        return self.__count_keys(self.owner._ptr)

    def __getitem__(self, key):
        if not isinstance(key, str): raise TypeError
        ckey = cstr(key)
        #if self.__contains(self.owner._ptr, ckey) != 1:
        #    raise KeyError('no such key: %s' % (key))
        return StringListAccess(self.owner,
            lambda owner: self.__count_values(owner, ckey),
            lambda owner, d, o, cnt: self.__get_values(owner, ckey, d, o, cnt),
            lambda owner, v: self.__add(owner, ckey, v),
            lambda owner, s: self.__del_kv(owner, ckey, s),
            lambda owner, i: self.__del_ki(owner, ckey, i),
            lambda owner, i, v: self.__set_ki(owner, ckey, i, v))

    def __delitem__(self, key):
        if not isinstance(key, str): raise TypeError
        ckey = cstr(key)
        while lib.pkg_info_del_i(self._ptr, ckey, 0) == 1: pass

    def __contains__(self, key):
        return (True if self.__contains(self.owner._ptr, cstr(key))
                else False)

    def __iter__(self):
        def iter():
            for key in self.get_keys():
                yield key
        return iter
    def __reversed__(self):
        def iter():
            for key in reverse(self.get_keys()):
                yield key
        return iter

    def get_keys(self, offset=None, count=None):
        avail = self.__count_keys(self.owner._ptr)
        if offset is None:
            offset = 0
            if count is None:
                count = avail
        elif count is None:
            count = 1
        if count > avail - offset:
            count = avail - offset
        lst = (ctypes.c_char_p * count)()
        got = self.__get_keys(self.owner._ptr, lst, offset, count)
        return [ from_c_string(x) for x in lst[0:got] ]
