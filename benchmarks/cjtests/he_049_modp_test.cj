import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve(3, 5), 3)
}

@Test
func case1() {
    @Expect(typed_fn_solve(1101, 101), 2)
}

@Test
func case2() {
    @Expect(typed_fn_solve(0, 101), 1)
}

@Test
func case3() {
    @Expect(typed_fn_solve(3, 11), 8)
}

@Test
func case4() {
    @Expect(typed_fn_solve(100, 101), 1)
}

@Test
func case5() {
    @Expect(typed_fn_solve(30, 5), 4)
}

@Test
func case6() {
    @Expect(typed_fn_solve(31, 5), 3)
}

@Test
func case7() {
    @Expect(typed_fn_solve(0, 2), 1)
}

@Test
func case8() {
    @Expect(typed_fn_solve(1, 2), 0)
}

@Test
func case9() {
    @Expect(typed_fn_solve(2, 2), 0)
}

@Test
func case10() {
    @Expect(typed_fn_solve(10, 3), 1)
}

@Test
func case11() {
    @Expect(typed_fn_solve(127, 97), 66)
}

@Test
func case12() {
    @Expect(typed_fn_solve(500, 1009), 946)
}

@Test
func case13() {
    @Expect(typed_fn_solve(47, 76), 72)
}

@Test
func case14() {
    @Expect(typed_fn_solve(83, 55), 8)
}

@Test
func case15() {
    @Expect(typed_fn_solve(50, 94), 16)
}

@Test
func case16() {
    @Expect(typed_fn_solve(6, 70), 64)
}

@Test
func case17() {
    @Expect(typed_fn_solve(65, 83), 72)
}

@Test
func case18() {
    @Expect(typed_fn_solve(66, 35), 29)
}

@Test
func case19() {
    @Expect(typed_fn_solve(39, 40), 8)
}

@Test
func case20() {
    @Expect(typed_fn_solve(16, 9), 7)
}

@Test
func case21() {
    @Expect(typed_fn_solve(42, 88), 48)
}

@Test
func case22() {
    @Expect(typed_fn_solve(50, 65), 4)
}

@Test
func case23() {
    @Expect(typed_fn_solve(86, 92), 52)
}

@Test
func case24() {
    @Expect(typed_fn_solve(28, 36), 16)
}

@Test
func case25() {
    @Expect(typed_fn_solve(1, 56), 2)
}

@Test
func case26() {
    @Expect(typed_fn_solve(59, 83), 53)
}

@Test
func case27() {
    @Expect(typed_fn_solve(64, 99), 16)
}

@Test
func case28() {
    @Expect(typed_fn_solve(69, 87), 14)
}

@Test
func case29() {
    @Expect(typed_fn_solve(38, 95), 4)
}

@Test
func case30() {
    @Expect(typed_fn_solve(23, 26), 20)
}

@Test
func case31() {
    @Expect(typed_fn_solve(80, 90), 76)
}

@Test
func case32() {
    @Expect(typed_fn_solve(60, 59), 4)
}
