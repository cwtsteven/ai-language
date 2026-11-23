import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve(3, 7), 1)
}

@Test
func case1() {
    @Expect(typed_fn_solve(10, 15), 5)
}

@Test
func case2() {
    @Expect(typed_fn_solve(49, 14), 7)
}

@Test
func case3() {
    @Expect(typed_fn_solve(144, 60), 12)
}

@Test
func case4() {
    @Expect(typed_fn_solve(0, 1), 1)
}

@Test
func case5() {
    @Expect(typed_fn_solve(1, 0), 1)
}

@Test
func case6() {
    @Expect(typed_fn_solve(0, 99), 99)
}

@Test
func case7() {
    @Expect(typed_fn_solve(99, 0), 99)
}

@Test
func case8() {
    @Expect(typed_fn_solve(7, 7), 7)
}

@Test
func case9() {
    @Expect(typed_fn_solve(1, 999), 1)
}

@Test
func case10() {
    @Expect(typed_fn_solve(100, 25), 25)
}

@Test
func case11() {
    @Expect(typed_fn_solve(387, 295), 1)
}

@Test
func case12() {
    @Expect(typed_fn_solve(460, 494), 2)
}

@Test
func case13() {
    @Expect(typed_fn_solve(336, 253), 1)
}

@Test
func case14() {
    @Expect(typed_fn_solve(82, 89), 1)
}

@Test
func case15() {
    @Expect(typed_fn_solve(417, 287), 1)
}

@Test
func case16() {
    @Expect(typed_fn_solve(46, 277), 1)
}

@Test
func case17() {
    @Expect(typed_fn_solve(437, 194), 1)
}

@Test
func case18() {
    @Expect(typed_fn_solve(199, 377), 1)
}

@Test
func case19() {
    @Expect(typed_fn_solve(96, 16), 16)
}

@Test
func case20() {
    @Expect(typed_fn_solve(336, 38), 2)
}

@Test
func case21() {
    @Expect(typed_fn_solve(52, 218), 2)
}

@Test
func case22() {
    @Expect(typed_fn_solve(335, 140), 5)
}

@Test
func case23() {
    @Expect(typed_fn_solve(373, 57), 1)
}

@Test
func case24() {
    @Expect(typed_fn_solve(346, 412), 2)
}

@Test
func case25() {
    @Expect(typed_fn_solve(28, 145), 1)
}

@Test
func case26() {
    @Expect(typed_fn_solve(176, 239), 1)
}

@Test
func case27() {
    @Expect(typed_fn_solve(339, 168), 3)
}

@Test
func case28() {
    @Expect(typed_fn_solve(24, 486), 6)
}

@Test
func case29() {
    @Expect(typed_fn_solve(32, 389), 1)
}

@Test
func case30() {
    @Expect(typed_fn_solve(140, 26), 2)
}
