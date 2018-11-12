

	1. SQL injection:
The following will execute a SQL injection attacK


	Execution of attack:
There are two ways to execute the attack:
a. Use wget:
 wget --save-cookies cookies.txt --post-data="FormName=Login&Login=user'#&Password=dad&FormAction=login" http://127.0.0.1:8080/bookstore/Login.jsp
     - This downloads the user page with user logged in

b. Manually login:
 In the login prompt, type:
    Login: user'#
    Password: 

	Why this attack works:

1. On using user'#, the rest of the SQL query is commented out. toSQL(sLogin, adText) does not correcly escape this character.In effect, we have changed the query to "select member_id, member_level from members where member_login = user" and we have completely skipped the password checking.

	Prevention:

Since there are a varierty of SQL attacks possible and we do not want to blacklist like toSQL does, we use a Prepared Statement. Since a prepared statement is compiled, there is no way, we can change the semantics of the query. See file "Attack Prevention" for how to prevent this attack by applying the correct patch.



=======================================================================================================================================

	2. XSS:

	Execution of attack:
a. session hijacking, a
defacement, and also redirecting the browser to another address.
