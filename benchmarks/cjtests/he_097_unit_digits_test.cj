import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve(148, 412), 16)
}

@Test
func case1() {
    @Expect(typed_fn_solve(19, 28), 72)
}

@Test
func case2() {
    @Expect(typed_fn_solve(2020, 1851), 0)
}

@Test
func case3() {
    @Expect(typed_fn_solve(14, -15), 20)
}

@Test
func case4() {
    @Expect(typed_fn_solve(76, 67), 42)
}

@Test
func case5() {
    @Expect(typed_fn_solve(17, 27), 49)
}

@Test
func case6() {
    @Expect(typed_fn_solve(0, 1), 0)
}

@Test
func case7() {
    @Expect(typed_fn_solve(0, 0), 0)
}

@Test
func case8() {
    @Expect(typed_fn_solve(-1, -1), 1)
}

@Test
func case9() {
    @Expect(typed_fn_solve(-19, 28), 72)
}

@Test
func case10() {
    @Expect(typed_fn_solve(10000, -10000), 0)
}

@Test
func case11() {
    @Expect(typed_fn_solve(9999, 9999), 81)
}

@Test
func case12() {
    @Expect(typed_fn_solve(595, 4955), 25)
}

@Test
func case13() {
    @Expect(typed_fn_solve(9528, -5091), 8)
}

@Test
func case14() {
    @Expect(typed_fn_solve(8116, 1826), 36)
}

@Test
func case15() {
    @Expect(typed_fn_solve(2533, 9105), 15)
}

@Test
func case16() {
    @Expect(typed_fn_solve(9173, 8509), 27)
}

@Test
func case17() {
    @Expect(typed_fn_solve(-8569, -6868), 72)
}

@Test
func case18() {
    @Expect(typed_fn_solve(3385, 6483), 15)
}

@Test
func case19() {
    @Expect(typed_fn_solve(-7251, 290), 0)
}

@Test
func case20() {
    @Expect(typed_fn_solve(-5811, -6096), 6)
}

@Test
func case21() {
    @Expect(typed_fn_solve(954, -3843), 12)
}

@Test
func case22() {
    @Expect(typed_fn_solve(648, 6606), 48)
}

@Test
func case23() {
    @Expect(typed_fn_solve(9635, 6567), 35)
}

@Test
func case24() {
    @Expect(typed_fn_solve(8555, 3716), 30)
}

@Test
func case25() {
    @Expect(typed_fn_solve(410, -6953), 0)
}

@Test
func case26() {
    @Expect(typed_fn_solve(-1962, -9175), 10)
}

@Test
func case27() {
    @Expect(typed_fn_solve(-3687, -579), 63)
}

@Test
func case28() {
    @Expect(typed_fn_solve(7317, -8136), 42)
}

@Test
func case29() {
    @Expect(typed_fn_solve(-7010, -1130), 0)
}

@Test
func case30() {
    @Expect(typed_fn_solve(-2949, 4175), 45)
}

@Test
func case31() {
    @Expect(typed_fn_solve(8933, 6523), 9)
}
