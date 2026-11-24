import std.collection.ArrayList
import std.unittest.*
import std.unittest.testmacro.*

func listOf(items: Array<Int64>): ArrayList<Int64> {
    let out = ArrayList<Int64>()
    for (item in items) {
        out.add(item)
    }
    return out
}

@Test
func case0() {
    @Expect(typed_fn_solve(2, 10), listOf([2, 4, 6, 8]))
}

@Test
func case1() {
    @Expect(typed_fn_solve(10, 2), listOf([2, 4, 6, 8]))
}

@Test
func case2() {
    @Expect(typed_fn_solve(132, 2), listOf([2, 4, 6, 8]))
}

@Test
func case3() {
    @Expect(typed_fn_solve(17, 89), listOf([]))
}

@Test
func case4() {
    @Expect(typed_fn_solve(8, 8), listOf([8]))
}

@Test
func case5() {
    @Expect(typed_fn_solve(1, 1), listOf([]))
}

@Test
func case6() {
    @Expect(typed_fn_solve(3, 7), listOf([4, 6]))
}

@Test
func case7() {
    @Expect(typed_fn_solve(9, 9), listOf([]))
}

@Test
func case8() {
    @Expect(typed_fn_solve(2, 2), listOf([2]))
}
