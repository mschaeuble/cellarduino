package tk.freeax.sensorserver;

import com.querydsl.core.Tuple;
import com.querydsl.core.types.Expression;
import com.querydsl.sql.*;
import com.querydsl.sql.dml.SQLDeleteClause;
import com.querydsl.sql.dml.SQLInsertClause;
import org.sqlite.SQLiteDataSource;

abstract class BaseRepository {

    private final SQLQueryFactory queryFactory;

    public BaseRepository(String databaseUrl) {
        SQLTemplates templates = new SQLiteTemplates();
        Configuration configuration = new Configuration(templates);
        SQLiteDataSource dataSource = new SQLiteDataSource();
        dataSource.setUrl(databaseUrl);
        queryFactory = new SQLQueryFactory(configuration, dataSource);
    }

    protected SQLQuery<Tuple> select(Expression... exprs) {
        return queryFactory.select(exprs);
    }

    protected SQLDeleteClause delete(RelationalPath<?> path) {
        return queryFactory.delete(path);
    }

    protected SQLInsertClause insert(RelationalPath<?> path) {
        return queryFactory.insert(path);
    }

}
