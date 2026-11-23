import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve(1), 1)
}

@Test
func case1() {
    @Expect(typed_fn_solve(6), 21)
}

@Test
func case2() {
    @Expect(typed_fn_solve(11), 66)
}

@Test
func case3() {
    @Expect(typed_fn_solve(30), 465)
}

@Test
func case4() {
    @Expect(typed_fn_solve(100), 5050)
}

@Test
func case5() {
    @Expect(typed_fn_solve(0), 0)
}

@Test
func case6() {
    @Expect(typed_fn_solve(2), 3)
}

@Test
func case7() {
    @Expect(typed_fn_solve(3), 6)
}

@Test
func case8() {
    @Expect(typed_fn_solve(4), 10)
}

@Test
func case9() {
    @Expect(typed_fn_solve(5), 15)
}

@Test
func case10() {
    @Expect(typed_fn_solve(7), 28)
}

@Test
func case11() {
    @Expect(typed_fn_solve(10), 55)
}

@Test
func case12() {
    @Expect(typed_fn_solve(99), 4950)
}

@Test
func case13() {
    @Expect(typed_fn_solve(101), 5151)
}

@Test
func case14() {
    @Expect(typed_fn_solve(256), 32896)
}

@Test
func case15() {
    @Expect(typed_fn_solve(999), 499500)
}

@Test
func case16() {
    @Expect(typed_fn_solve(1000), 500500)
}

@Test
func case17() {
    @Expect(typed_fn_solve(497), 123753)
}

@Test
func case18() {
    @Expect(typed_fn_solve(203), 20706)
}

@Test
func case19() {
    @Expect(typed_fn_solve(674), 227475)
}

@Test
func case20() {
    @Expect(typed_fn_solve(255), 32640)
}

@Test
func case21() {
    @Expect(typed_fn_solve(913), 417241)
}

@Test
func case22() {
    @Expect(typed_fn_solve(411), 84666)
}

@Test
func case23() {
    @Expect(typed_fn_solve(342), 58653)
}

@Test
func case24() {
    @Expect(typed_fn_solve(266), 35511)
}

@Test
func case25() {
    @Expect(typed_fn_solve(801), 321201)
}

@Test
func case26() {
    @Expect(typed_fn_solve(620), 192510)
}

@Test
func case27() {
    @Expect(typed_fn_solve(313), 49141)
}

@Test
func case28() {
    @Expect(typed_fn_solve(278), 38781)
}

@Test
func case29() {
    @Expect(typed_fn_solve(23), 276)
}

@Test
func case30() {
    @Expect(typed_fn_solve(860), 370230)
}

@Test
func case31() {
    @Expect(typed_fn_solve(324), 52650)
}

@Test
func case32() {
    @Expect(typed_fn_solve(338), 57291)
}

@Test
func case33() {
    @Expect(typed_fn_solve(829), 344035)
}

@Test
func case34() {
    @Expect(typed_fn_solve(362), 65703)
}

@Test
func case35() {
    @Expect(typed_fn_solve(219), 24090)
}

@Test
func case36() {
    @Expect(typed_fn_solve(817), 334153)
}
