# BEMRosetta python functions list

class BEMRosetta:
    def __init__(self, path_dll):
        self.libc = ctypes.CDLL(dll)

        # INPUT TYPES
        self.libc.DemoVectorPy_C.argtypes = [np.ctypeslib.ndpointer(dtype=np.float64), ctypes.c_int]
        self.libc.DLL_FAST_Load.argtypes = [ctypes.c_char_p]
        self.libc.DLL_FAST_GetParameterName.argtypes = [ctypes.c_int]
        self.libc.DLL_FAST_GetUnitName.argtypes = [ctypes.c_int]
        self.libc.DLL_FAST_GetParameterId.argtypes = [ctypes.c_char_p]
        self.libc.DLL_FAST_GetTime.argtypes = [ctypes.c_int]
        self.libc.DLL_FAST_GetData.argtypes = [ctypes.c_int, ctypes.c_int]
        self.libc.DLL_FAST_GetAvg.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
        self.libc.DLL_FAST_GetMax.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
        self.libc.DLL_FAST_GetMin.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
        self.libc.DLL_FAST_GetArray.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.POINTER(ctypes.c_double)), ctypes.POINTER(ctypes.c_int)]
        self.libc.DLL_FAST_LoadFile.argtypes = [ctypes.c_char_p]
        self.libc.DLL_FAST_SaveFile.argtypes = [ctypes.c_char_p]
        self.libc.DLL_FAST_SetVar.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
        self.libc.DLL_FAST_GetVar.argtypes = [ctypes.c_char_p, ctypes.c_char_p]

        # OUTPUT TYPES
        self.libc.DemoVectorPy_C.restype = ctypes.c_double
        self.libc.DLL_Version.restype = ctypes.c_char_p
        self.libc.DLL_strListFunctions.restype = ctypes.c_char_p
        self.libc.DLL_strPythonDeclaration.restype = ctypes.c_char_p
        self.libc.DLL_FAST_Load.restype = ctypes.c_int
        self.libc.DLL_FAST_GetParameterName.restype = ctypes.c_char_p
        self.libc.DLL_FAST_GetUnitName.restype = ctypes.c_char_p
        self.libc.DLL_FAST_GetParameterId.restype = ctypes.c_int
        self.libc.DLL_FAST_GetParameterCount.restype = ctypes.c_int
        self.libc.DLL_FAST_GetLen.restype = ctypes.c_int
        self.libc.DLL_FAST_GetTimeStart.restype = ctypes.c_double
        self.libc.DLL_FAST_GetTimeEnd.restype = ctypes.c_double
        self.libc.DLL_FAST_GetTime.restype = ctypes.c_double
        self.libc.DLL_FAST_GetData.restype = ctypes.c_double
        self.libc.DLL_FAST_GetAvg.restype = ctypes.c_double
        self.libc.DLL_FAST_GetMax.restype = ctypes.c_double
        self.libc.DLL_FAST_GetMin.restype = ctypes.c_double
        self.libc.DLL_FAST_GetArray.restype = ctypes.c_int
        self.libc.DLL_FAST_LoadFile.restype = ctypes.c_int
        self.libc.DLL_FAST_SaveFile.restype = ctypes.c_int
        self.libc.DLL_FAST_SetVar.restype = ctypes.c_int
        self.libc.DLL_FAST_GetVar.restype = ctypes.c_char_p

    def DemoVectorPy_C(self, v):
        self.libc.DemoVectorPy_C(v, len(v))

    def FAST_Load(self, filename):
        self.libc.DLL_FAST_Load(str.encode(filename, 'UTF-8'))

    def FAST_GetParameterName(self, id):
        self.libc.DLL_FAST_GetParameterName(id)

    def FAST_GetUnitName(self, id):
        self.libc.DLL_FAST_GetUnitName(id)

    def FAST_GetParameterId(self, name):
        self.libc.DLL_FAST_GetParameterId(str.encode(name, 'UTF-8'))

    def FAST_GetTime(self, idtime):
        self.libc.DLL_FAST_GetTime(idtime)

    def FAST_GetData(self, idtime, idparam):
        self.libc.DLL_FAST_GetData(idtime, idparam)

    def FAST_GetAvg(self, idparam, idbegin, idend):
        self.libc.DLL_FAST_GetAvg(idparam, idbegin, idend)

    def FAST_GetMax(self, idparam, idbegin, idend):
        self.libc.DLL_FAST_GetMax(idparam, idbegin, idend)

    def FAST_GetMin(self, idparam, idbegin, idend):
        self.libc.DLL_FAST_GetMin(idparam, idbegin, idend)

    def FAST_GetArray(self, idparam, idbegin, idend):
        # Argument preparation
        _data0 = ctypes.POINTER(ctypes.c_double)()
        _size0 = ctypes.c_int()
        # DLL function call
        ret = self.libc.DLL_FAST_GetArray(idparam, idbegin, idend, ctypes.byref(_data0), ctypes.byref(_size0))
        # Vector processing
        _arraySize0 = ctypes.c_double * _size0.value
        _data0_pointer = ctypes.cast(_data0, ctypes.POINTER(_arraySize0))
		data = np.frombuffer(_data0_pointer.contents)
        return ret, data

    def FAST_LoadFile(self, file):
        self.libc.DLL_FAST_LoadFile(str.encode(file, 'UTF-8'))

    def FAST_SaveFile(self, file):
        self.libc.DLL_FAST_SaveFile(str.encode(file, 'UTF-8'))

    def FAST_SetVar(self, name, paragraph, value):
        self.libc.DLL_FAST_SetVar(str.encode(name, 'UTF-8'), str.encode(paragraph, 'UTF-8'), str.encode(value, 'UTF-8'))

    def FAST_GetVar(self, name, paragraph):
        self.libc.DLL_FAST_GetVar(str.encode(name, 'UTF-8'), str.encode(paragraph, 'UTF-8'))