import sys

class Slitherlink:
    ROWS = 6
    COLS = 6

    def __init__(self):
        self.h_edges = [[0 for _ in range(Slitherlink.COLS)] for _ in range(Slitherlink.ROWS + 1)]
        self.v_edges = [[0 for _ in range(Slitherlink.COLS + 1)] for _ in range(Slitherlink.ROWS)]
        self.r = 0
        self.c = 0

    def copy(self):
        o = Slitherlink()
        o.h_edges = [x[:] for x in self.h_edges]
        o.v_edges = [x[:] for x in self.v_edges]
        o.r = self.r
        o.c = self.c
        return o

    def is_right_valid(self):
        if self.c == Slitherlink.COLS or \
                self.h_edges[self.r][self.c] == 1:
                    return False        
        elif self.r > 0 and self.r < Slitherlink.ROWS:
            if self.v_edges[self.r - 1][self.c + 1] == 1 and \
                    self.v_edges[self.r][self.c + 1] == 1:
                        return False
            else:
                return True
        else:
            return True

    def is_left_valid(self):
        if self.c == 0 or self.h_edges[self.r][self.c - 1] == 1:
            return False
        elif self.r > 0 and self.r < Slitherlink.ROWS:
            if self.v_edges[self.r - 1][self.c - 1] == 1 and \
                    self.v_edges[self.r][self.c - 1] == 1:
                        return False
            else:
                return True
        else:
            return True

    def is_down_valid(self):
        if self.r == Slitherlink.COLS or \
                self.v_edges[self.r][self.c] == 1:
                    return False
        elif self.c > 0 and self.c < Slitherlink.COLS:
            if self.h_edges[self.r + 1][self.c - 1] == 1 and \
                    self.h_edges[self.r + 1][self.c] == 1:
                        return False
            else:
                return True
        else:
            return True

    def is_up_valid(self):
        if self.r == 0 or self.v_edges[self.r - 1][self.c] == 1:
            return False
        elif self.c > 0 and self.c < Slitherlink.COLS:
            if self.h_edges[self.r - 1][self.c - 1] == 1 and \
                    self.h_edges[self.r - 1][self.c] == 1:
                        return False
            else:
                return True
        else:
            return True

    def move_right(self):
        assert(self.c < Slitherlink.COLS)
        self.h_edges[self.r][self.c] = 1
        self.c += 1

    def move_left(self):
        assert(self.c > 0)
        self.c -= 1
        self.h_edges[self.r][self.c] = 1

    def move_down(self):
        assert(self.r < Slitherlink.ROWS)
        self.v_edges[self.r][self.c] = 1
        self.r += 1

    def move_up(self):
        assert(self.r > 0)
        self.r -= 1
        self.v_edges[self.r][self.c] = 1

    def show(self):
        for i in range(Slitherlink.ROWS+1):
            for j in range(Slitherlink.COLS):
                if self.h_edges[i][j] == 1:
                    sys.stdout.write(' --')
                else:
                    sys.stdout.write('   ')
            sys.stdout.write('\n')

            if i < Slitherlink.ROWS:
                for j in range(Slitherlink.COLS + 1):
                    if self.v_edges[i][j] == 1:
                        sys.stdout.write('|  ')
                    else:
                        sys.stdout.write('   ')
                sys.stdout.write('\n')

def solve(p, counter):
    valid = [
        p.is_left_valid(),
        p.is_right_valid(),
        p.is_up_valid(),
        p.is_down_valid()
    ]

    if not any(valid):
        counter[0] += 1
        if counter[0] % 1000000 == 0:
            print(counter[0])
        #p.show()
        #sys.stdout.write('\n')        
    else:
        if valid[0]:
            p1 = p.copy()
            p1.move_left()
            solve(p1, counter)
        if valid[1]:
            p1 = p.copy()
            p1.move_right()
            solve(p1, counter)
        if valid[2]:
            p1 = p.copy()
            p1.move_up()
            solve(p1, counter)
        if valid[3]:
            p1 = p.copy()
            p1.move_down()
            solve(p1, counter)

p = Slitherlink()
counter = [0]
solve(p, counter)
