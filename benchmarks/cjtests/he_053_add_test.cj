import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve(0, 1), 1)
}

@Test
func case1() {
    @Expect(typed_fn_solve(1, 0), 1)
}

@Test
func case2() {
    @Expect(typed_fn_solve(2, 3), 5)
}

@Test
func case3() {
    @Expect(typed_fn_solve(5, 7), 12)
}

@Test
func case4() {
    @Expect(typed_fn_solve(7, 5), 12)
}

@Test
func case5() {
    @Expect(typed_fn_solve(0, 0), 0)
}

@Test
func case6() {
    @Expect(typed_fn_solve(-1, 1), 0)
}

@Test
func case7() {
    @Expect(typed_fn_solve(-7, -9), -16)
}

@Test
func case8() {
    @Expect(typed_fn_solve(-23, 5), -18)
}

@Test
func case9() {
    @Expect(typed_fn_solve(9223372036854775806, 1), 9223372036854775807)
}

@Test
func case10() {
    @Expect(typed_fn_solve(-9223372036854775808, 0), -9223372036854775808)
}

@Test
func case11() {
    @Expect(typed_fn_solve(145, 427), 572)
}

@Test
func case12() {
    @Expect(typed_fn_solve(391, 300), 691)
}

@Test
func case13() {
    @Expect(typed_fn_solve(837, -649), 188)
}

@Test
func case14() {
    @Expect(typed_fn_solve(683, -167), 516)
}

@Test
func case15() {
    @Expect(typed_fn_solve(933, 483), 1416)
}

@Test
func case16() {
    @Expect(typed_fn_solve(-930, -742), -1672)
}

@Test
func case17() {
    @Expect(typed_fn_solve(664, -469), 195)
}

@Test
func case18() {
    @Expect(typed_fn_solve(381, -749), -368)
}

@Test
func case19() {
    @Expect(typed_fn_solve(424, 998), 1422)
}

@Test
func case20() {
    @Expect(typed_fn_solve(-124, 82), -42)
}

@Test
func case21() {
    @Expect(typed_fn_solve(-513, -305), -818)
}

@Test
func case22() {
    @Expect(typed_fn_solve(-666, 467), -199)
}

@Test
func case23() {
    @Expect(typed_fn_solve(433, 301), 734)
}

@Test
func case24() {
    @Expect(typed_fn_solve(-397, -486), -883)
}

@Test
func case25() {
    @Expect(typed_fn_solve(874, 791), 1665)
}

@Test
func case26() {
    @Expect(typed_fn_solve(61, -318), -257)
}

@Test
func case27() {
    @Expect(typed_fn_solve(-176, -620), -796)
}

@Test
func case28() {
    @Expect(typed_fn_solve(926, 106), 1032)
}

@Test
func case29() {
    @Expect(typed_fn_solve(-108, -369), -477)
}

@Test
func case30() {
    @Expect(typed_fn_solve(343, -570), -227)
}

@Test
func case31() {
    @Expect(typed_fn_solve(391, 263), 654)
}

@Test
func case32() {
    @Expect(typed_fn_solve(-964, -173), -1137)
}

@Test
func case33() {
    @Expect(typed_fn_solve(-541, -909), -1450)
}

@Test
func case34() {
    @Expect(typed_fn_solve(840, 700), 1540)
}

@Test
func case35() {
    @Expect(typed_fn_solve(16, -856), -840)
}

@Test
func case36() {
    @Expect(typed_fn_solve(-544, -807), -1351)
}

@Test
func case37() {
    @Expect(typed_fn_solve(-877, -320), -1197)
}

@Test
func case38() {
    @Expect(typed_fn_solve(-940, -196), -1136)
}

@Test
func case39() {
    @Expect(typed_fn_solve(-50, -517), -567)
}

@Test
func case40() {
    @Expect(typed_fn_solve(934, -588), 346)
}
