import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve(7, 34, 12), 34)
}

@Test
func case1() {
    @Expect(typed_fn_solve(15, 8, 5), 5)
}

@Test
func case2() {
    @Expect(typed_fn_solve(3, 33, 5212), 33)
}

@Test
func case3() {
    @Expect(typed_fn_solve(1259, 3, 52), 3)
}

@Test
func case4() {
    @Expect(typed_fn_solve(7919, -1, 12), -1)
}

@Test
func case5() {
    @Expect(typed_fn_solve(3609, 1245, 583), 583)
}

@Test
func case6() {
    @Expect(typed_fn_solve(91, 56, 129), 129)
}

@Test
func case7() {
    @Expect(typed_fn_solve(6, 34, 1234), 1234)
}

@Test
func case8() {
    @Expect(typed_fn_solve(1, 2, 0), 0)
}

@Test
func case9() {
    @Expect(typed_fn_solve(2, 2, 0), 2)
}

@Test
func case10() {
    @Expect(typed_fn_solve(-3, 9, -4), -4)
}

@Test
func case11() {
    @Expect(typed_fn_solve(0, -5, 6), 6)
}

@Test
func case12() {
    @Expect(typed_fn_solve(2, -10, -20), -10)
}

@Test
func case13() {
    @Expect(typed_fn_solve(4, 3, -7), -7)
}

@Test
func case14() {
    @Expect(typed_fn_solve(49, 1, 2), 2)
}

@Test
func case15() {
    @Expect(typed_fn_solve(97, 8, 9), 8)
}

@Test
func case16() {
    @Expect(typed_fn_solve(69, -672, 772), 772)
}

@Test
func case17() {
    @Expect(typed_fn_solve(169, -98, 113), 113)
}

@Test
func case18() {
    @Expect(typed_fn_solve(37, 293, -459), 293)
}

@Test
func case19() {
    @Expect(typed_fn_solve(89, 841, 200), 841)
}

@Test
func case20() {
    @Expect(typed_fn_solve(57, 530, 3), 3)
}

@Test
func case21() {
    @Expect(typed_fn_solve(64, -263, -380), -380)
}

@Test
func case22() {
    @Expect(typed_fn_solve(61, -337, 339), -337)
}

@Test
func case23() {
    @Expect(typed_fn_solve(101, 259, 318), 259)
}

@Test
func case24() {
    @Expect(typed_fn_solve(154, -176, 453), 453)
}

@Test
func case25() {
    @Expect(typed_fn_solve(78, -490, -929), -929)
}

@Test
func case26() {
    @Expect(typed_fn_solve(67, -491, -202), -491)
}

@Test
func case27() {
    @Expect(typed_fn_solve(159, -817, 492), 492)
}

@Test
func case28() {
    @Expect(typed_fn_solve(45, 767, 868), 868)
}

@Test
func case29() {
    @Expect(typed_fn_solve(58, -758, -662), -662)
}

@Test
func case30() {
    @Expect(typed_fn_solve(39, 592, 380), 380)
}

@Test
func case31() {
    @Expect(typed_fn_solve(179, 796, -141), 796)
}

@Test
func case32() {
    @Expect(typed_fn_solve(177, -810, -190), -190)
}

@Test
func case33() {
    @Expect(typed_fn_solve(25, 484, 288), 288)
}

@Test
func case34() {
    @Expect(typed_fn_solve(191, -594, 992), -594)
}

@Test
func case35() {
    @Expect(typed_fn_solve(134, -559, -83), -83)
}
