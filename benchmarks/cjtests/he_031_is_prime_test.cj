import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve(6), false)
}

@Test
func case1() {
    @Expect(typed_fn_solve(101), true)
}

@Test
func case2() {
    @Expect(typed_fn_solve(11), true)
}

@Test
func case3() {
    @Expect(typed_fn_solve(13441), true)
}

@Test
func case4() {
    @Expect(typed_fn_solve(61), true)
}

@Test
func case5() {
    @Expect(typed_fn_solve(4), false)
}

@Test
func case6() {
    @Expect(typed_fn_solve(1), false)
}

@Test
func case7() {
    @Expect(typed_fn_solve(5), true)
}

@Test
func case8() {
    @Expect(typed_fn_solve(17), true)
}

@Test
func case9() {
    @Expect(typed_fn_solve(85), false)
}

@Test
func case10() {
    @Expect(typed_fn_solve(77), false)
}

@Test
func case11() {
    @Expect(typed_fn_solve(255379), false)
}

@Test
func case12() {
    @Expect(typed_fn_solve(-17), false)
}

@Test
func case13() {
    @Expect(typed_fn_solve(-1), false)
}

@Test
func case14() {
    @Expect(typed_fn_solve(0), false)
}

@Test
func case15() {
    @Expect(typed_fn_solve(2), true)
}

@Test
func case16() {
    @Expect(typed_fn_solve(3), true)
}

@Test
func case17() {
    @Expect(typed_fn_solve(9), false)
}

@Test
func case18() {
    @Expect(typed_fn_solve(25), false)
}

@Test
func case19() {
    @Expect(typed_fn_solve(49), false)
}

@Test
func case20() {
    @Expect(typed_fn_solve(97), true)
}

@Test
func case21() {
    @Expect(typed_fn_solve(121), false)
}

@Test
func case22() {
    @Expect(typed_fn_solve(169), false)
}

@Test
func case23() {
    @Expect(typed_fn_solve(997), true)
}

@Test
func case24() {
    @Expect(typed_fn_solve(243), false)
}

@Test
func case25() {
    @Expect(typed_fn_solve(423), false)
}

@Test
func case26() {
    @Expect(typed_fn_solve(444), false)
}

@Test
func case27() {
    @Expect(typed_fn_solve(389), true)
}

@Test
func case28() {
    @Expect(typed_fn_solve(291), false)
}

@Test
func case29() {
    @Expect(typed_fn_solve(122), false)
}

@Test
func case30() {
    @Expect(typed_fn_solve(102), false)
}

@Test
func case31() {
    @Expect(typed_fn_solve(426), false)
}

@Test
func case32() {
    @Expect(typed_fn_solve(458), false)
}

@Test
func case33() {
    @Expect(typed_fn_solve(373), true)
}

@Test
func case34() {
    @Expect(typed_fn_solve(246), false)
}

@Test
func case35() {
    @Expect(typed_fn_solve(303), false)
}

@Test
func case36() {
    @Expect(typed_fn_solve(60), false)
}

@Test
func case37() {
    @Expect(typed_fn_solve(377), false)
}

@Test
func case38() {
    @Expect(typed_fn_solve(328), false)
}

@Test
func case39() {
    @Expect(typed_fn_solve(166), false)
}

@Test
func case40() {
    @Expect(typed_fn_solve(411), false)
}

@Test
func case41() {
    @Expect(typed_fn_solve(211), true)
}

@Test
func case42() {
    @Expect(typed_fn_solve(209), false)
}

@Test
func case43() {
    @Expect(typed_fn_solve(267), false)
}
