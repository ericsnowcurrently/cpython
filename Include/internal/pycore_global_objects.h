#ifndef Py_INTERNAL_GLOBAL_OBJECTS_H
#define Py_INTERNAL_GLOBAL_OBJECTS_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif


#define _PyObject_IMMORTAL_INIT(type) \
    { \
        .ob_refcnt = 999999999, \
        .ob_type = type, \
    }
#define _PyVarObject_IMMORTAL_INIT(type, size) \
    { \
        .ob_base = _PyObject_IMMORTAL_INIT(type), \
        .ob_size = size, \
    }


/* int objects */

#define _PY_NSMALLPOSINTS           257
#define _PY_NSMALLNEGINTS           5

#define _PyLong_DIGIT_INIT(val) \
    { \
        _PyVarObject_IMMORTAL_INIT(&PyLong_Type, \
                                   ((val) == 0 ? 0 : ((val) > 0 ? 1 : -1))), \
        .ob_digit = { ((val) >= 0 ? (val) : -(val)) }, \
    }


/* unicode objects */

#define _PyASCII_BYTE_INIT(len, ASCII) \
    { \
        .ob_base = _PyObject_IMMORTAL_INIT(&PyUnicode_Type), \
        .length = len, \
        .hash = -1, \
        .state = { \
            .kind = PyUnicode_1BYTE_KIND, \
            .compact = 1, \
            .ascii = ASCII, \
            .ready = 1, \
        }, \
    }

#define _PyASCIIObject_FULL(len) \
    struct _PyASCIIObject_ ## len { \
        PyASCIIObject ascii; \
        uint8_t data[len + 1]; \
    }
#define _PyASCIIObject_FULL_INIT(LITERAL) \
    { \
        .ascii = _PyASCII_BYTE_INIT(Py_ARRAY_LENGTH((LITERAL)) - 1, 1), \
        .data = LITERAL, \
    }

#define _PyLatin1Object_FULL(len) \
    struct _PyLatin1Object_ ## len { \
        PyCompactUnicodeObject compact; \
        uint8_t data[len + 1]; \
    }
#define _PyLatin1Object_FULL_INIT(LITERAL) \
    { \
        .compact = { \
            ._base = _PyASCII_BYTE_INIT(Py_ARRAY_LENGTH((LITERAL)) - 1, 0), \
        }, \
        .data = LITERAL, \
    }

static inline void
_PyUnicode_reset(PyASCIIObject *op)
{
    // Force a new hash to be generated since the hash seed may have changed.
    op->hash = -1;
}


/**********************
 * the global objects *
 **********************/

// Only immutable objects should be considered runtime-global.
// All others must be per-interpreter.

#define _Py_GLOBAL_OBJECT(NAME) \
    _PyRuntime.global_objects.NAME
#define _Py_SINGLETON(NAME) \
    _Py_GLOBAL_OBJECT(singletons.NAME)

struct _Py_global_objects {
    struct {
        /* Small integers are preallocated in this array so that they
         * can be shared.
         * The integers that are preallocated are those in the range
         * -_PY_NSMALLNEGINTS (inclusive) to _PY_NSMALLPOSINTS (exclusive).
         */
        PyLongObject small_ints[_PY_NSMALLNEGINTS + _PY_NSMALLPOSINTS];

        // The empty Unicode object is a singleton to improve performance.
        _PyASCIIObject_FULL(0) unicode_empty;
        /* Single character Unicode strings in the Latin-1 range are being
           shared as well. */
        _PyASCIIObject_FULL(1) unicode_ascii[128];
        _PyLatin1Object_FULL(1) unicode_latin1[128];
    } singletons;
};

#define _Py_global_objects_INIT { \
    .singletons = { \
        .small_ints = { \
            _PyLong_DIGIT_INIT(-5), \
            _PyLong_DIGIT_INIT(-4), \
            _PyLong_DIGIT_INIT(-3), \
            _PyLong_DIGIT_INIT(-2), \
            _PyLong_DIGIT_INIT(-1), \
            _PyLong_DIGIT_INIT(0), \
            _PyLong_DIGIT_INIT(1), \
            _PyLong_DIGIT_INIT(2), \
            _PyLong_DIGIT_INIT(3), \
            _PyLong_DIGIT_INIT(4), \
            _PyLong_DIGIT_INIT(5), \
            _PyLong_DIGIT_INIT(6), \
            _PyLong_DIGIT_INIT(7), \
            _PyLong_DIGIT_INIT(8), \
            _PyLong_DIGIT_INIT(9), \
            _PyLong_DIGIT_INIT(10), \
            _PyLong_DIGIT_INIT(11), \
            _PyLong_DIGIT_INIT(12), \
            _PyLong_DIGIT_INIT(13), \
            _PyLong_DIGIT_INIT(14), \
            _PyLong_DIGIT_INIT(15), \
            _PyLong_DIGIT_INIT(16), \
            _PyLong_DIGIT_INIT(17), \
            _PyLong_DIGIT_INIT(18), \
            _PyLong_DIGIT_INIT(19), \
            _PyLong_DIGIT_INIT(20), \
            _PyLong_DIGIT_INIT(21), \
            _PyLong_DIGIT_INIT(22), \
            _PyLong_DIGIT_INIT(23), \
            _PyLong_DIGIT_INIT(24), \
            _PyLong_DIGIT_INIT(25), \
            _PyLong_DIGIT_INIT(26), \
            _PyLong_DIGIT_INIT(27), \
            _PyLong_DIGIT_INIT(28), \
            _PyLong_DIGIT_INIT(29), \
            _PyLong_DIGIT_INIT(30), \
            _PyLong_DIGIT_INIT(31), \
            _PyLong_DIGIT_INIT(32), \
            _PyLong_DIGIT_INIT(33), \
            _PyLong_DIGIT_INIT(34), \
            _PyLong_DIGIT_INIT(35), \
            _PyLong_DIGIT_INIT(36), \
            _PyLong_DIGIT_INIT(37), \
            _PyLong_DIGIT_INIT(38), \
            _PyLong_DIGIT_INIT(39), \
            _PyLong_DIGIT_INIT(40), \
            _PyLong_DIGIT_INIT(41), \
            _PyLong_DIGIT_INIT(42), \
            _PyLong_DIGIT_INIT(43), \
            _PyLong_DIGIT_INIT(44), \
            _PyLong_DIGIT_INIT(45), \
            _PyLong_DIGIT_INIT(46), \
            _PyLong_DIGIT_INIT(47), \
            _PyLong_DIGIT_INIT(48), \
            _PyLong_DIGIT_INIT(49), \
            _PyLong_DIGIT_INIT(50), \
            _PyLong_DIGIT_INIT(51), \
            _PyLong_DIGIT_INIT(52), \
            _PyLong_DIGIT_INIT(53), \
            _PyLong_DIGIT_INIT(54), \
            _PyLong_DIGIT_INIT(55), \
            _PyLong_DIGIT_INIT(56), \
            _PyLong_DIGIT_INIT(57), \
            _PyLong_DIGIT_INIT(58), \
            _PyLong_DIGIT_INIT(59), \
            _PyLong_DIGIT_INIT(60), \
            _PyLong_DIGIT_INIT(61), \
            _PyLong_DIGIT_INIT(62), \
            _PyLong_DIGIT_INIT(63), \
            _PyLong_DIGIT_INIT(64), \
            _PyLong_DIGIT_INIT(65), \
            _PyLong_DIGIT_INIT(66), \
            _PyLong_DIGIT_INIT(67), \
            _PyLong_DIGIT_INIT(68), \
            _PyLong_DIGIT_INIT(69), \
            _PyLong_DIGIT_INIT(70), \
            _PyLong_DIGIT_INIT(71), \
            _PyLong_DIGIT_INIT(72), \
            _PyLong_DIGIT_INIT(73), \
            _PyLong_DIGIT_INIT(74), \
            _PyLong_DIGIT_INIT(75), \
            _PyLong_DIGIT_INIT(76), \
            _PyLong_DIGIT_INIT(77), \
            _PyLong_DIGIT_INIT(78), \
            _PyLong_DIGIT_INIT(79), \
            _PyLong_DIGIT_INIT(80), \
            _PyLong_DIGIT_INIT(81), \
            _PyLong_DIGIT_INIT(82), \
            _PyLong_DIGIT_INIT(83), \
            _PyLong_DIGIT_INIT(84), \
            _PyLong_DIGIT_INIT(85), \
            _PyLong_DIGIT_INIT(86), \
            _PyLong_DIGIT_INIT(87), \
            _PyLong_DIGIT_INIT(88), \
            _PyLong_DIGIT_INIT(89), \
            _PyLong_DIGIT_INIT(90), \
            _PyLong_DIGIT_INIT(91), \
            _PyLong_DIGIT_INIT(92), \
            _PyLong_DIGIT_INIT(93), \
            _PyLong_DIGIT_INIT(94), \
            _PyLong_DIGIT_INIT(95), \
            _PyLong_DIGIT_INIT(96), \
            _PyLong_DIGIT_INIT(97), \
            _PyLong_DIGIT_INIT(98), \
            _PyLong_DIGIT_INIT(99), \
            _PyLong_DIGIT_INIT(100), \
            _PyLong_DIGIT_INIT(101), \
            _PyLong_DIGIT_INIT(102), \
            _PyLong_DIGIT_INIT(103), \
            _PyLong_DIGIT_INIT(104), \
            _PyLong_DIGIT_INIT(105), \
            _PyLong_DIGIT_INIT(106), \
            _PyLong_DIGIT_INIT(107), \
            _PyLong_DIGIT_INIT(108), \
            _PyLong_DIGIT_INIT(109), \
            _PyLong_DIGIT_INIT(110), \
            _PyLong_DIGIT_INIT(111), \
            _PyLong_DIGIT_INIT(112), \
            _PyLong_DIGIT_INIT(113), \
            _PyLong_DIGIT_INIT(114), \
            _PyLong_DIGIT_INIT(115), \
            _PyLong_DIGIT_INIT(116), \
            _PyLong_DIGIT_INIT(117), \
            _PyLong_DIGIT_INIT(118), \
            _PyLong_DIGIT_INIT(119), \
            _PyLong_DIGIT_INIT(120), \
            _PyLong_DIGIT_INIT(121), \
            _PyLong_DIGIT_INIT(122), \
            _PyLong_DIGIT_INIT(123), \
            _PyLong_DIGIT_INIT(124), \
            _PyLong_DIGIT_INIT(125), \
            _PyLong_DIGIT_INIT(126), \
            _PyLong_DIGIT_INIT(127), \
            _PyLong_DIGIT_INIT(128), \
            _PyLong_DIGIT_INIT(129), \
            _PyLong_DIGIT_INIT(130), \
            _PyLong_DIGIT_INIT(131), \
            _PyLong_DIGIT_INIT(132), \
            _PyLong_DIGIT_INIT(133), \
            _PyLong_DIGIT_INIT(134), \
            _PyLong_DIGIT_INIT(135), \
            _PyLong_DIGIT_INIT(136), \
            _PyLong_DIGIT_INIT(137), \
            _PyLong_DIGIT_INIT(138), \
            _PyLong_DIGIT_INIT(139), \
            _PyLong_DIGIT_INIT(140), \
            _PyLong_DIGIT_INIT(141), \
            _PyLong_DIGIT_INIT(142), \
            _PyLong_DIGIT_INIT(143), \
            _PyLong_DIGIT_INIT(144), \
            _PyLong_DIGIT_INIT(145), \
            _PyLong_DIGIT_INIT(146), \
            _PyLong_DIGIT_INIT(147), \
            _PyLong_DIGIT_INIT(148), \
            _PyLong_DIGIT_INIT(149), \
            _PyLong_DIGIT_INIT(150), \
            _PyLong_DIGIT_INIT(151), \
            _PyLong_DIGIT_INIT(152), \
            _PyLong_DIGIT_INIT(153), \
            _PyLong_DIGIT_INIT(154), \
            _PyLong_DIGIT_INIT(155), \
            _PyLong_DIGIT_INIT(156), \
            _PyLong_DIGIT_INIT(157), \
            _PyLong_DIGIT_INIT(158), \
            _PyLong_DIGIT_INIT(159), \
            _PyLong_DIGIT_INIT(160), \
            _PyLong_DIGIT_INIT(161), \
            _PyLong_DIGIT_INIT(162), \
            _PyLong_DIGIT_INIT(163), \
            _PyLong_DIGIT_INIT(164), \
            _PyLong_DIGIT_INIT(165), \
            _PyLong_DIGIT_INIT(166), \
            _PyLong_DIGIT_INIT(167), \
            _PyLong_DIGIT_INIT(168), \
            _PyLong_DIGIT_INIT(169), \
            _PyLong_DIGIT_INIT(170), \
            _PyLong_DIGIT_INIT(171), \
            _PyLong_DIGIT_INIT(172), \
            _PyLong_DIGIT_INIT(173), \
            _PyLong_DIGIT_INIT(174), \
            _PyLong_DIGIT_INIT(175), \
            _PyLong_DIGIT_INIT(176), \
            _PyLong_DIGIT_INIT(177), \
            _PyLong_DIGIT_INIT(178), \
            _PyLong_DIGIT_INIT(179), \
            _PyLong_DIGIT_INIT(180), \
            _PyLong_DIGIT_INIT(181), \
            _PyLong_DIGIT_INIT(182), \
            _PyLong_DIGIT_INIT(183), \
            _PyLong_DIGIT_INIT(184), \
            _PyLong_DIGIT_INIT(185), \
            _PyLong_DIGIT_INIT(186), \
            _PyLong_DIGIT_INIT(187), \
            _PyLong_DIGIT_INIT(188), \
            _PyLong_DIGIT_INIT(189), \
            _PyLong_DIGIT_INIT(190), \
            _PyLong_DIGIT_INIT(191), \
            _PyLong_DIGIT_INIT(192), \
            _PyLong_DIGIT_INIT(193), \
            _PyLong_DIGIT_INIT(194), \
            _PyLong_DIGIT_INIT(195), \
            _PyLong_DIGIT_INIT(196), \
            _PyLong_DIGIT_INIT(197), \
            _PyLong_DIGIT_INIT(198), \
            _PyLong_DIGIT_INIT(199), \
            _PyLong_DIGIT_INIT(200), \
            _PyLong_DIGIT_INIT(201), \
            _PyLong_DIGIT_INIT(202), \
            _PyLong_DIGIT_INIT(203), \
            _PyLong_DIGIT_INIT(204), \
            _PyLong_DIGIT_INIT(205), \
            _PyLong_DIGIT_INIT(206), \
            _PyLong_DIGIT_INIT(207), \
            _PyLong_DIGIT_INIT(208), \
            _PyLong_DIGIT_INIT(209), \
            _PyLong_DIGIT_INIT(210), \
            _PyLong_DIGIT_INIT(211), \
            _PyLong_DIGIT_INIT(212), \
            _PyLong_DIGIT_INIT(213), \
            _PyLong_DIGIT_INIT(214), \
            _PyLong_DIGIT_INIT(215), \
            _PyLong_DIGIT_INIT(216), \
            _PyLong_DIGIT_INIT(217), \
            _PyLong_DIGIT_INIT(218), \
            _PyLong_DIGIT_INIT(219), \
            _PyLong_DIGIT_INIT(220), \
            _PyLong_DIGIT_INIT(221), \
            _PyLong_DIGIT_INIT(222), \
            _PyLong_DIGIT_INIT(223), \
            _PyLong_DIGIT_INIT(224), \
            _PyLong_DIGIT_INIT(225), \
            _PyLong_DIGIT_INIT(226), \
            _PyLong_DIGIT_INIT(227), \
            _PyLong_DIGIT_INIT(228), \
            _PyLong_DIGIT_INIT(229), \
            _PyLong_DIGIT_INIT(230), \
            _PyLong_DIGIT_INIT(231), \
            _PyLong_DIGIT_INIT(232), \
            _PyLong_DIGIT_INIT(233), \
            _PyLong_DIGIT_INIT(234), \
            _PyLong_DIGIT_INIT(235), \
            _PyLong_DIGIT_INIT(236), \
            _PyLong_DIGIT_INIT(237), \
            _PyLong_DIGIT_INIT(238), \
            _PyLong_DIGIT_INIT(239), \
            _PyLong_DIGIT_INIT(240), \
            _PyLong_DIGIT_INIT(241), \
            _PyLong_DIGIT_INIT(242), \
            _PyLong_DIGIT_INIT(243), \
            _PyLong_DIGIT_INIT(244), \
            _PyLong_DIGIT_INIT(245), \
            _PyLong_DIGIT_INIT(246), \
            _PyLong_DIGIT_INIT(247), \
            _PyLong_DIGIT_INIT(248), \
            _PyLong_DIGIT_INIT(249), \
            _PyLong_DIGIT_INIT(250), \
            _PyLong_DIGIT_INIT(251), \
            _PyLong_DIGIT_INIT(252), \
            _PyLong_DIGIT_INIT(253), \
            _PyLong_DIGIT_INIT(254), \
            _PyLong_DIGIT_INIT(255), \
            _PyLong_DIGIT_INIT(256), \
        }, \
        \
        .unicode_empty = _PyASCIIObject_FULL_INIT(""), \
        .unicode_ascii = { \
            _PyASCIIObject_FULL_INIT("\x00"), \
            _PyASCIIObject_FULL_INIT("\x01"), \
            _PyASCIIObject_FULL_INIT("\x02"), \
            _PyASCIIObject_FULL_INIT("\x03"), \
            _PyASCIIObject_FULL_INIT("\x04"), \
            _PyASCIIObject_FULL_INIT("\x05"), \
            _PyASCIIObject_FULL_INIT("\x06"), \
            _PyASCIIObject_FULL_INIT("\x07"), \
            _PyASCIIObject_FULL_INIT("\x08"), \
            _PyASCIIObject_FULL_INIT("\x09"), \
            _PyASCIIObject_FULL_INIT("\x0a"), \
            _PyASCIIObject_FULL_INIT("\x0b"), \
            _PyASCIIObject_FULL_INIT("\x0c"), \
            _PyASCIIObject_FULL_INIT("\x0d"), \
            _PyASCIIObject_FULL_INIT("\x0e"), \
            _PyASCIIObject_FULL_INIT("\x0f"), \
            _PyASCIIObject_FULL_INIT("\x10"), \
            _PyASCIIObject_FULL_INIT("\x11"), \
            _PyASCIIObject_FULL_INIT("\x12"), \
            _PyASCIIObject_FULL_INIT("\x13"), \
            _PyASCIIObject_FULL_INIT("\x14"), \
            _PyASCIIObject_FULL_INIT("\x15"), \
            _PyASCIIObject_FULL_INIT("\x16"), \
            _PyASCIIObject_FULL_INIT("\x17"), \
            _PyASCIIObject_FULL_INIT("\x18"), \
            _PyASCIIObject_FULL_INIT("\x19"), \
            _PyASCIIObject_FULL_INIT("\x1a"), \
            _PyASCIIObject_FULL_INIT("\x1b"), \
            _PyASCIIObject_FULL_INIT("\x1c"), \
            _PyASCIIObject_FULL_INIT("\x1d"), \
            _PyASCIIObject_FULL_INIT("\x1e"), \
            _PyASCIIObject_FULL_INIT("\x1f"), \
            _PyASCIIObject_FULL_INIT("\x20"), \
            _PyASCIIObject_FULL_INIT("\x21"), \
            _PyASCIIObject_FULL_INIT("\x22"), \
            _PyASCIIObject_FULL_INIT("\x23"), \
            _PyASCIIObject_FULL_INIT("\x24"), \
            _PyASCIIObject_FULL_INIT("\x25"), \
            _PyASCIIObject_FULL_INIT("\x26"), \
            _PyASCIIObject_FULL_INIT("\x27"), \
            _PyASCIIObject_FULL_INIT("\x28"), \
            _PyASCIIObject_FULL_INIT("\x29"), \
            _PyASCIIObject_FULL_INIT("\x2a"), \
            _PyASCIIObject_FULL_INIT("\x2b"), \
            _PyASCIIObject_FULL_INIT("\x2c"), \
            _PyASCIIObject_FULL_INIT("\x2d"), \
            _PyASCIIObject_FULL_INIT("\x2e"), \
            _PyASCIIObject_FULL_INIT("\x2f"), \
            _PyASCIIObject_FULL_INIT("\x30"), \
            _PyASCIIObject_FULL_INIT("\x31"), \
            _PyASCIIObject_FULL_INIT("\x32"), \
            _PyASCIIObject_FULL_INIT("\x33"), \
            _PyASCIIObject_FULL_INIT("\x34"), \
            _PyASCIIObject_FULL_INIT("\x35"), \
            _PyASCIIObject_FULL_INIT("\x36"), \
            _PyASCIIObject_FULL_INIT("\x37"), \
            _PyASCIIObject_FULL_INIT("\x38"), \
            _PyASCIIObject_FULL_INIT("\x39"), \
            _PyASCIIObject_FULL_INIT("\x3a"), \
            _PyASCIIObject_FULL_INIT("\x3b"), \
            _PyASCIIObject_FULL_INIT("\x3c"), \
            _PyASCIIObject_FULL_INIT("\x3d"), \
            _PyASCIIObject_FULL_INIT("\x3e"), \
            _PyASCIIObject_FULL_INIT("\x3f"), \
            _PyASCIIObject_FULL_INIT("\x40"), \
            _PyASCIIObject_FULL_INIT("\x41"), \
            _PyASCIIObject_FULL_INIT("\x42"), \
            _PyASCIIObject_FULL_INIT("\x43"), \
            _PyASCIIObject_FULL_INIT("\x44"), \
            _PyASCIIObject_FULL_INIT("\x45"), \
            _PyASCIIObject_FULL_INIT("\x46"), \
            _PyASCIIObject_FULL_INIT("\x47"), \
            _PyASCIIObject_FULL_INIT("\x48"), \
            _PyASCIIObject_FULL_INIT("\x49"), \
            _PyASCIIObject_FULL_INIT("\x4a"), \
            _PyASCIIObject_FULL_INIT("\x4b"), \
            _PyASCIIObject_FULL_INIT("\x4c"), \
            _PyASCIIObject_FULL_INIT("\x4d"), \
            _PyASCIIObject_FULL_INIT("\x4e"), \
            _PyASCIIObject_FULL_INIT("\x4f"), \
            _PyASCIIObject_FULL_INIT("\x50"), \
            _PyASCIIObject_FULL_INIT("\x51"), \
            _PyASCIIObject_FULL_INIT("\x52"), \
            _PyASCIIObject_FULL_INIT("\x53"), \
            _PyASCIIObject_FULL_INIT("\x54"), \
            _PyASCIIObject_FULL_INIT("\x55"), \
            _PyASCIIObject_FULL_INIT("\x56"), \
            _PyASCIIObject_FULL_INIT("\x57"), \
            _PyASCIIObject_FULL_INIT("\x58"), \
            _PyASCIIObject_FULL_INIT("\x59"), \
            _PyASCIIObject_FULL_INIT("\x5a"), \
            _PyASCIIObject_FULL_INIT("\x5b"), \
            _PyASCIIObject_FULL_INIT("\x5c"), \
            _PyASCIIObject_FULL_INIT("\x5d"), \
            _PyASCIIObject_FULL_INIT("\x5e"), \
            _PyASCIIObject_FULL_INIT("\x5f"), \
            _PyASCIIObject_FULL_INIT("\x60"), \
            _PyASCIIObject_FULL_INIT("\x61"), \
            _PyASCIIObject_FULL_INIT("\x62"), \
            _PyASCIIObject_FULL_INIT("\x63"), \
            _PyASCIIObject_FULL_INIT("\x64"), \
            _PyASCIIObject_FULL_INIT("\x65"), \
            _PyASCIIObject_FULL_INIT("\x66"), \
            _PyASCIIObject_FULL_INIT("\x67"), \
            _PyASCIIObject_FULL_INIT("\x68"), \
            _PyASCIIObject_FULL_INIT("\x69"), \
            _PyASCIIObject_FULL_INIT("\x6a"), \
            _PyASCIIObject_FULL_INIT("\x6b"), \
            _PyASCIIObject_FULL_INIT("\x6c"), \
            _PyASCIIObject_FULL_INIT("\x6d"), \
            _PyASCIIObject_FULL_INIT("\x6e"), \
            _PyASCIIObject_FULL_INIT("\x6f"), \
            _PyASCIIObject_FULL_INIT("\x70"), \
            _PyASCIIObject_FULL_INIT("\x71"), \
            _PyASCIIObject_FULL_INIT("\x72"), \
            _PyASCIIObject_FULL_INIT("\x73"), \
            _PyASCIIObject_FULL_INIT("\x74"), \
            _PyASCIIObject_FULL_INIT("\x75"), \
            _PyASCIIObject_FULL_INIT("\x76"), \
            _PyASCIIObject_FULL_INIT("\x77"), \
            _PyASCIIObject_FULL_INIT("\x78"), \
            _PyASCIIObject_FULL_INIT("\x79"), \
            _PyASCIIObject_FULL_INIT("\x7a"), \
            _PyASCIIObject_FULL_INIT("\x7b"), \
            _PyASCIIObject_FULL_INIT("\x7c"), \
            _PyASCIIObject_FULL_INIT("\x7d"), \
            _PyASCIIObject_FULL_INIT("\x7e"), \
            _PyASCIIObject_FULL_INIT("\x7f"), \
        }, \
        .unicode_latin1 = { \
            _PyLatin1Object_FULL_INIT("\x80"), \
            _PyLatin1Object_FULL_INIT("\x81"), \
            _PyLatin1Object_FULL_INIT("\x82"), \
            _PyLatin1Object_FULL_INIT("\x83"), \
            _PyLatin1Object_FULL_INIT("\x84"), \
            _PyLatin1Object_FULL_INIT("\x85"), \
            _PyLatin1Object_FULL_INIT("\x86"), \
            _PyLatin1Object_FULL_INIT("\x87"), \
            _PyLatin1Object_FULL_INIT("\x88"), \
            _PyLatin1Object_FULL_INIT("\x89"), \
            _PyLatin1Object_FULL_INIT("\x8a"), \
            _PyLatin1Object_FULL_INIT("\x8b"), \
            _PyLatin1Object_FULL_INIT("\x8c"), \
            _PyLatin1Object_FULL_INIT("\x8d"), \
            _PyLatin1Object_FULL_INIT("\x8e"), \
            _PyLatin1Object_FULL_INIT("\x8f"), \
            _PyLatin1Object_FULL_INIT("\x90"), \
            _PyLatin1Object_FULL_INIT("\x91"), \
            _PyLatin1Object_FULL_INIT("\x92"), \
            _PyLatin1Object_FULL_INIT("\x93"), \
            _PyLatin1Object_FULL_INIT("\x94"), \
            _PyLatin1Object_FULL_INIT("\x95"), \
            _PyLatin1Object_FULL_INIT("\x96"), \
            _PyLatin1Object_FULL_INIT("\x97"), \
            _PyLatin1Object_FULL_INIT("\x98"), \
            _PyLatin1Object_FULL_INIT("\x99"), \
            _PyLatin1Object_FULL_INIT("\x9a"), \
            _PyLatin1Object_FULL_INIT("\x9b"), \
            _PyLatin1Object_FULL_INIT("\x9c"), \
            _PyLatin1Object_FULL_INIT("\x9d"), \
            _PyLatin1Object_FULL_INIT("\x9e"), \
            _PyLatin1Object_FULL_INIT("\x9f"), \
            _PyLatin1Object_FULL_INIT("\xa0"), \
            _PyLatin1Object_FULL_INIT("\xa1"), \
            _PyLatin1Object_FULL_INIT("\xa2"), \
            _PyLatin1Object_FULL_INIT("\xa3"), \
            _PyLatin1Object_FULL_INIT("\xa4"), \
            _PyLatin1Object_FULL_INIT("\xa5"), \
            _PyLatin1Object_FULL_INIT("\xa6"), \
            _PyLatin1Object_FULL_INIT("\xa7"), \
            _PyLatin1Object_FULL_INIT("\xa8"), \
            _PyLatin1Object_FULL_INIT("\xa9"), \
            _PyLatin1Object_FULL_INIT("\xaa"), \
            _PyLatin1Object_FULL_INIT("\xab"), \
            _PyLatin1Object_FULL_INIT("\xac"), \
            _PyLatin1Object_FULL_INIT("\xad"), \
            _PyLatin1Object_FULL_INIT("\xae"), \
            _PyLatin1Object_FULL_INIT("\xaf"), \
            _PyLatin1Object_FULL_INIT("\xb0"), \
            _PyLatin1Object_FULL_INIT("\xb1"), \
            _PyLatin1Object_FULL_INIT("\xb2"), \
            _PyLatin1Object_FULL_INIT("\xb3"), \
            _PyLatin1Object_FULL_INIT("\xb4"), \
            _PyLatin1Object_FULL_INIT("\xb5"), \
            _PyLatin1Object_FULL_INIT("\xb6"), \
            _PyLatin1Object_FULL_INIT("\xb7"), \
            _PyLatin1Object_FULL_INIT("\xb8"), \
            _PyLatin1Object_FULL_INIT("\xb9"), \
            _PyLatin1Object_FULL_INIT("\xba"), \
            _PyLatin1Object_FULL_INIT("\xbb"), \
            _PyLatin1Object_FULL_INIT("\xbc"), \
            _PyLatin1Object_FULL_INIT("\xbd"), \
            _PyLatin1Object_FULL_INIT("\xbe"), \
            _PyLatin1Object_FULL_INIT("\xbf"), \
            _PyLatin1Object_FULL_INIT("\xc0"), \
            _PyLatin1Object_FULL_INIT("\xc1"), \
            _PyLatin1Object_FULL_INIT("\xc2"), \
            _PyLatin1Object_FULL_INIT("\xc3"), \
            _PyLatin1Object_FULL_INIT("\xc4"), \
            _PyLatin1Object_FULL_INIT("\xc5"), \
            _PyLatin1Object_FULL_INIT("\xc6"), \
            _PyLatin1Object_FULL_INIT("\xc7"), \
            _PyLatin1Object_FULL_INIT("\xc8"), \
            _PyLatin1Object_FULL_INIT("\xc9"), \
            _PyLatin1Object_FULL_INIT("\xca"), \
            _PyLatin1Object_FULL_INIT("\xcb"), \
            _PyLatin1Object_FULL_INIT("\xcc"), \
            _PyLatin1Object_FULL_INIT("\xcd"), \
            _PyLatin1Object_FULL_INIT("\xce"), \
            _PyLatin1Object_FULL_INIT("\xcf"), \
            _PyLatin1Object_FULL_INIT("\xd0"), \
            _PyLatin1Object_FULL_INIT("\xd1"), \
            _PyLatin1Object_FULL_INIT("\xd2"), \
            _PyLatin1Object_FULL_INIT("\xd3"), \
            _PyLatin1Object_FULL_INIT("\xd4"), \
            _PyLatin1Object_FULL_INIT("\xd5"), \
            _PyLatin1Object_FULL_INIT("\xd6"), \
            _PyLatin1Object_FULL_INIT("\xd7"), \
            _PyLatin1Object_FULL_INIT("\xd8"), \
            _PyLatin1Object_FULL_INIT("\xd9"), \
            _PyLatin1Object_FULL_INIT("\xda"), \
            _PyLatin1Object_FULL_INIT("\xdb"), \
            _PyLatin1Object_FULL_INIT("\xdc"), \
            _PyLatin1Object_FULL_INIT("\xdd"), \
            _PyLatin1Object_FULL_INIT("\xde"), \
            _PyLatin1Object_FULL_INIT("\xdf"), \
            _PyLatin1Object_FULL_INIT("\xe0"), \
            _PyLatin1Object_FULL_INIT("\xe1"), \
            _PyLatin1Object_FULL_INIT("\xe2"), \
            _PyLatin1Object_FULL_INIT("\xe3"), \
            _PyLatin1Object_FULL_INIT("\xe4"), \
            _PyLatin1Object_FULL_INIT("\xe5"), \
            _PyLatin1Object_FULL_INIT("\xe6"), \
            _PyLatin1Object_FULL_INIT("\xe7"), \
            _PyLatin1Object_FULL_INIT("\xe8"), \
            _PyLatin1Object_FULL_INIT("\xe9"), \
            _PyLatin1Object_FULL_INIT("\xea"), \
            _PyLatin1Object_FULL_INIT("\xeb"), \
            _PyLatin1Object_FULL_INIT("\xec"), \
            _PyLatin1Object_FULL_INIT("\xed"), \
            _PyLatin1Object_FULL_INIT("\xee"), \
            _PyLatin1Object_FULL_INIT("\xef"), \
            _PyLatin1Object_FULL_INIT("\xf0"), \
            _PyLatin1Object_FULL_INIT("\xf1"), \
            _PyLatin1Object_FULL_INIT("\xf2"), \
            _PyLatin1Object_FULL_INIT("\xf3"), \
            _PyLatin1Object_FULL_INIT("\xf4"), \
            _PyLatin1Object_FULL_INIT("\xf5"), \
            _PyLatin1Object_FULL_INIT("\xf6"), \
            _PyLatin1Object_FULL_INIT("\xf7"), \
            _PyLatin1Object_FULL_INIT("\xf8"), \
            _PyLatin1Object_FULL_INIT("\xf9"), \
            _PyLatin1Object_FULL_INIT("\xfa"), \
            _PyLatin1Object_FULL_INIT("\xfb"), \
            _PyLatin1Object_FULL_INIT("\xfc"), \
            _PyLatin1Object_FULL_INIT("\xfd"), \
            _PyLatin1Object_FULL_INIT("\xfe"), \
            _PyLatin1Object_FULL_INIT("\xff"), \
        }, \
    }, \
}

static inline void
_Py_global_objects_reset(struct _Py_global_objects *objects)
{
    _PyUnicode_reset(&objects->singletons.unicode_empty.ascii);
    for (int i = 0; i < 128 + 1; i++) {
        _PyUnicode_reset(&objects->singletons.unicode_ascii[i].ascii);
    }
    for (int i = 128; i < 256; i++) {
        _PyUnicode_reset(&objects->singletons.unicode_latin1[i].compact._base);
    }
}

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_GLOBAL_OBJECTS_H */
