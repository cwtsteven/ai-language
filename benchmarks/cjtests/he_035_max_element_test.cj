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
    @Expect(typed_fn_solve(listOf([1, 2, 3])), 3)
}

@Test
func case1() {
    @Expect(typed_fn_solve(listOf([5, 3, -5, 2, -3, 3, 9, 0, 124, 1, -10])), 124)
}

@Test
func case2() {
    @Expect(typed_fn_solve(listOf([7])), 7)
}

@Test
func case3() {
    @Expect(typed_fn_solve(listOf([-4, -1, -8])), -1)
}

@Test
func case4() {
    @Expect(typed_fn_solve(listOf([0, 0])), 0)
}

@Test
func case5() {
    @Expect(typed_fn_solve(listOf([150, -20])), 150)
}
